// SPDX-License-Identifier: GPL-3.0-or-later
#include "airbridge.h"
#include <algorithm>
#include <cctype>
#include <cerrno>
#include <climits>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "nawigacja.h"
#include "wspolne.h"

// ---------- JSON ----------
// double → int/long (32 bity na ESP32) tylko w zakresie typu; poza nim zachowanie
// niezdefiniowane. Wartość spoza zakresu traktujemy jak brak pola.
static bool wInt(double v) { return v >= -2e9 && v <= 2e9; }
// double → float tylko w zakresie float; poza nim brak danych (NAN), nie zachowanie niezdefiniowane.
static float naFloat(double v) { return std::fabs(v) <= 3.0e38 ? (float)v : NAN; }
static const Json BRAK_JSON;
const FiltrJson TYLKO_KLUCZ[] = {{nullptr, nullptr}};

const Json& Json::operator[](const char* k) const {
  if (typ != OBIEKT) return BRAK_JSON;
  for (size_t i = 0; i < klucze.size(); i++)
    if (klucze[i] == k) return el[i];
  return BRAK_JSON;
}

const Json& Json::operator[](size_t i) const { return typ == TABLICA && i < el.size() ? el[i] : BRAK_JSON; }

bool Json::liczba(double& v) const {
  if (typ != LICZBA) return false;
  v = n;
  return true;
}

namespace {
struct Czytnik {
  const char* p;
  const char* k;
  int glebokosc = 0;

  void ws() {
    while (p < k && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')) p++;
  }
  bool znak(char c) {
    ws();
    if (p < k && *p == c) { p++; return true; }
    return false;
  }
  static void utf8(std::string& o, unsigned c) {
    if (c < 0x80) o += (char)c;
    else if (c < 0x800) { o += (char)(0xC0 | c >> 6); o += (char)(0x80 | (c & 0x3F)); }
    else if (c < 0x10000) { o += (char)(0xE0 | c >> 12); o += (char)(0x80 | (c >> 6 & 0x3F)); o += (char)(0x80 | (c & 0x3F)); }
    else {
      o += (char)(0xF0 | c >> 18); o += (char)(0x80 | (c >> 12 & 0x3F));
      o += (char)(0x80 | (c >> 6 & 0x3F)); o += (char)(0x80 | (c & 0x3F));
    }
  }
  bool hex4(unsigned& v) {
    if (k - p < 4) return false;
    v = 0;
    for (int i = 0; i < 4; i++, p++) {
      char c = *p;
      v <<= 4;
      if (c >= '0' && c <= '9') v |= c - '0';
      else if (c >= 'a' && c <= 'f') v |= c - 'a' + 10;
      else if (c >= 'A' && c <= 'F') v |= c - 'A' + 10;
      else return false;
    }
    return true;
  }
  bool tekst(std::string* o) {  // o == nullptr: tylko pomiń
    ws();
    if (p >= k || *p != '"') return false;
    p++;
    while (p < k && *p != '"') {
      char c = *p++;
      if ((unsigned char)c < 0x20) return false;
      if (c != '\\') { if (o) *o += c; continue; }
      if (p >= k) return false;
      char e = *p++;
      unsigned u;
      switch (e) {
        case '"': case '\\': case '/': if (o) *o += e; break;
        case 'b': if (o) *o += '\b'; break;
        case 'f': if (o) *o += '\f'; break;
        case 'n': if (o) *o += '\n'; break;
        case 'r': if (o) *o += '\r'; break;
        case 't': if (o) *o += '\t'; break;
        case 'u':
          if (!hex4(u)) return false;
          if (u >= 0xDC00 && u < 0xE000) return false;  // samotny drugi surogat
          if (u >= 0xD800 && u < 0xDC00) {               // pierwszy surogat: musi być para \uDC00–\uDFFF
            if (k - p < 6 || p[0] != '\\' || p[1] != 'u') return false;
            p += 2;
            unsigned d;
            if (!hex4(d) || d < 0xDC00 || d >= 0xE000) return false;
            u = 0x10000 + ((u - 0xD800) << 10) + (d - 0xDC00);
          }
          if (o) utf8(*o, u);
          break;
        default: return false;
      }
    }
    if (p >= k) return false;
    p++;
    return true;
  }
  bool slowo(const char* s) {
    size_t n = strlen(s);
    if ((size_t)(k - p) < n || strncmp(p, s, n) != 0) return false;
    p += n;
    return true;
  }
  // Liczba według gramatyki JSON (RFC 8259): -?(0|[1-9][0-9]*)(.[0-9]+)?([eE][+-]?[0-9]+)?; `01`, `1.`, `.5`, `+1`
  // i `1e` to błąd.
  bool liczba(double* o) {
    ws();
    const char* s = p;
    auto cyfra = [&]() { return p < k && *p >= '0' && *p <= '9'; };
    if (p < k && *p == '-') p++;
    if (!cyfra()) return false;
    if (*p == '0') p++;
    else while (cyfra()) p++;
    if (p < k && *p == '.') {
      p++;
      if (!cyfra()) return false;
      while (cyfra()) p++;
    }
    if (p < k && (*p == 'e' || *p == 'E')) {
      p++;
      if (p < k && (*p == '+' || *p == '-')) p++;
      if (!cyfra()) return false;
      while (cyfra()) p++;
    }
    if (o) {
      char b[48];
      size_t n = (size_t)(p - s);
      if (n >= sizeof b) {
        std::string t(s, n);
        *o = strtod(t.c_str(), nullptr);
      } else {
        memcpy(b, s, n);
        b[n] = 0;
        *o = strtod(b, nullptr);
      }
      // Gramatyka JSON przepuszcza `1e309` (strtod: nieskończoność) i liczby, których żadne pole AirBridge nie ma.
      // Poza ±AB_MAKS_LICZBY dokument jest nieczytelny; w zakresie każde rzutowanie na float jest określone.
      if (!(std::fabs(*o) <= AB_MAKS_LICZBY)) return false;
    }
    return true;
  }
  // Wartość do `o` (filtrowana) albo pominięcie (o == nullptr).
  bool wartosc(Json* o, const FiltrJson* f) {
    ws();
    if (p >= k || ++glebokosc > 24) return false;
    bool ok = true;
    char c = *p;
    if (c == '{') {
      p++;
      if (o) o->typ = Json::OBIEKT;
      if (!znak('}')) {
        do {
          std::string klucz;
          if (!tekst(&klucz) || !znak(':')) return false;
          const FiltrJson* pf = nullptr;
          bool bierz = o != nullptr;
          if (bierz && f) {
            bierz = false;
            for (const FiltrJson* x = f; x->klucz; x++)
              if (klucz == x->klucz) { bierz = true; pf = x->pod; break; }
          }
          if (bierz && pf == TYLKO_KLUCZ) {  // tylko obecność klucza: wartość sprawdzona i pominięta, bez alokacji
            o->klucze.push_back(klucz);
            o->el.emplace_back();
            o->el.back().typ = Json::NUL;
            if (!wartosc(nullptr, nullptr)) return false;
          } else if (bierz) {
            o->klucze.push_back(klucz);
            o->el.emplace_back();
            if (!wartosc(&o->el.back(), pf)) return false;
          } else if (!wartosc(nullptr, nullptr)) {
            return false;
          }
        } while (znak(','));
        if (!znak('}')) return false;
      }
    } else if (c == '[') {
      p++;
      if (o) o->typ = Json::TABLICA;
      if (!znak(']')) {
        do {
          if (o) {
            o->el.emplace_back();
            if (!wartosc(&o->el.back(), f)) return false;
          } else if (!wartosc(nullptr, nullptr)) {
            return false;
          }
        } while (znak(','));
        if (!znak(']')) return false;
      }
    } else if (c == '"') {
      if (o) o->typ = Json::TEKST;
      ok = tekst(o ? &o->s : nullptr);
    } else if (slowo("true")) {
      if (o) { o->typ = Json::BOOL; o->b = true; }
    } else if (slowo("false")) {
      if (o) { o->typ = Json::BOOL; o->b = false; }
    } else if (slowo("null")) {
      if (o) o->typ = Json::NUL;
    } else {
      if (o) o->typ = Json::LICZBA;
      ok = liczba(o ? &o->n : nullptr);
    }
    glebokosc--;
    return ok;
  }
};
}  // namespace

bool parsujJson(const std::string& t, Json& w, const FiltrJson* f) {
  w = Json();
  Czytnik c{t.data(), t.data() + t.size()};
  if (!c.wartosc(&w, f)) { w = Json(); return false; }
  c.ws();
  if (c.p != c.k) { w = Json(); return false; }
  return true;
}

// ---------- Digest ----------
WyzwanieDigest parsujWyzwanie(const std::string& h) {
  WyzwanieDigest w;
  size_t i = 0;
  while (i < h.size() && h[i] == ' ') i++;
  if (h.size() - i < 6) return w;
  std::string schemat = h.substr(i, 6);
  for (auto& c : schemat) c = (char)tolower((unsigned char)c);
  if (schemat != "digest") return w;
  i += 6;
  while (i < h.size()) {
    while (i < h.size() && (h[i] == ' ' || h[i] == ',')) i++;
    size_t r = h.find('=', i);
    if (r == std::string::npos) break;
    std::string klucz = h.substr(i, r - i);
    while (!klucz.empty() && klucz.back() == ' ') klucz.pop_back();
    for (auto& c : klucz) c = (char)tolower((unsigned char)c);
    i = r + 1;
    while (i < h.size() && h[i] == ' ') i++;
    std::string v;
    if (i < h.size() && h[i] == '"') {
      i++;
      while (i < h.size() && h[i] != '"') {
        if (h[i] == '\\' && i + 1 < h.size()) i++;
        v += h[i++];
      }
      i++;
    } else {
      while (i < h.size() && h[i] != ',') v += h[i++];
      while (!v.empty() && v.back() == ' ') v.pop_back();
    }
    if (klucz == "realm") w.realm = v;
    else if (klucz == "nonce") w.nonce = v;
    else if (klucz == "opaque") w.opaque = v;
    else if (klucz == "qop") w.qop = v;
    else if (klucz == "algorithm") w.algorytm = v;
  }
  w.ok = !w.nonce.empty();
  return w;
}

static bool maQopAuth(const std::string& qop) {  // lista "auth,auth-int": szukamy samego "auth"
  size_t i = 0;
  while (i <= qop.size()) {
    size_t j = qop.find(',', i);
    if (j == std::string::npos) j = qop.size();
    std::string t = qop.substr(i, j - i);
    t.erase(0, t.find_first_not_of(' '));
    while (!t.empty() && t.back() == ' ') t.pop_back();
    if (t == "auth") return true;
    i = j + 1;
  }
  return false;
}

std::string naglowekDigest(const WyzwanieDigest& w, const std::string& u, const std::string& haslo,
                           const std::string& metoda, const std::string& uri, unsigned nc, const std::string& cnonce,
                           Md5Hex md5) {
  char ncs[9];
  snprintf(ncs, sizeof ncs, "%08x", nc);
  bool qop = maQopAuth(w.qop);
  std::string alg = w.algorytm;
  for (auto& c : alg) c = (char)toupper((unsigned char)c);
  std::string ha1 = md5(u + ":" + w.realm + ":" + haslo);
  if (alg == "MD5-SESS") ha1 = md5(ha1 + ":" + w.nonce + ":" + cnonce);
  std::string ha2 = md5(metoda + ":" + uri);
  std::string odp = qop ? md5(ha1 + ":" + w.nonce + ":" + ncs + ":" + cnonce + ":auth:" + ha2)
                        : md5(ha1 + ":" + w.nonce + ":" + ha2);
  std::string h = "Digest username=\"" + u + "\", realm=\"" + w.realm + "\", nonce=\"" + w.nonce + "\", uri=\"" +
                  uri + "\"";
  if (!w.algorytm.empty()) h += ", algorithm=" + w.algorytm;
  h += ", response=\"" + odp + "\"";
  if (!w.opaque.empty()) h += ", opaque=\"" + w.opaque + "\"";
  if (qop) h += std::string(", qop=auth, nc=") + ncs + ", cnonce=\"" + cnonce + "\"";
  return h;
}

// ---------- status ----------
WersjaAB parsujWersje(const std::string& t) {
  WersjaAB w;
  size_t i = 0;
  if (i < t.size() && (t[i] == 'v' || t[i] == 'V')) i++;
  int* cz[3] = {&w.a, &w.b, &w.c};
  for (int k = 0; k < 3; k++) {
    if (k) {
      if (i >= t.size() || t[i] != '.') return WersjaAB();
      i++;
    }
    if (i >= t.size() || !isdigit((unsigned char)t[i])) return WersjaAB();
    int v = 0;
    while (i < t.size() && isdigit((unsigned char)t[i]) && v < 100000) v = v * 10 + (t[i++] - '0');
    *cz[k] = v;
  }
  return w;
}

bool wersjaObslugiwana(const WersjaAB& w) {
  if (!w.ok()) return false;
  return w.a > 2 || (w.a == 2 && w.b >= 2);
}

static std::string tekstPola(const Json& j) { return j.typ == Json::TEKST ? j.s : std::string(); }

bool parsujStatus(const std::string& t, StatusAB& s) {
  static const FiltrJson F[] = {{"version", nullptr},      {"pna", nullptr},          {"mode", nullptr},
                                {"system", nullptr},       {"rop", nullptr},          {"report_state", nullptr},
                                {"report_error", nullptr}, {"edf_prefix", nullptr},   {"edf", nullptr},
                                {"srn", nullptr},          {"esp_time", nullptr},     {nullptr, nullptr}};
  Json j;
  s = StatusAB();
  if (!parsujJson(t, j, F) || j.typ != Json::OBIEKT) return false;
  s.wersja = tekstPola(j["version"]);
  s.model = tekstPola(j["pna"]);
  s.tryb = tekstPola(j["mode"]);
  s.system = tekstPola(j["system"]);
  s.raport = tekstPola(j["report_state"]);
  s.bladRaportu = tekstPola(j["report_error"]);
  s.prefiks = tekstPola(j["edf_prefix"]);
  s.edf = tekstPola(j["edf"]);
  s.seryjny = tekstPola(j["srn"]);  // (R-NOR-25): zły typ albo brak = "" (brak odczytu)
  s.czas = tekstPola(j["esp_time"]);  // (R-AW-18)
  double r;
  if (j["rop"].liczba(r) && wInt(r)) s.rop = (int)r;
  return true;
}

bool parsujCzasAb(const std::string& t, std::tm& w) {
  if (t.size() != 16 || t[4] != '-' || t[7] != '-' || t[10] != ' ' || t[13] != ':') return false;
  for (size_t i = 0; i < t.size(); i++)
    if (i != 4 && i != 7 && i != 10 && i != 13 && !isdigit((unsigned char)t[i])) return false;
  int r = liczbaCalk(t.substr(0, 4).c_str()), mi = liczbaCalk(t.substr(5, 2).c_str()), d = liczbaCalk(t.substr(8, 2).c_str());
  int h = liczbaCalk(t.substr(11, 2).c_str()), mn = liczbaCalk(t.substr(14, 2).c_str());
  std::string data = t.substr(0, 10);
  if (!dataOk(data) || dataZNr(nrDnia(data)) != data || h > 23 || mn > 59) return false;
  w = std::tm();
  w.tm_year = r - 1900;
  w.tm_mon = mi - 1;
  w.tm_mday = d;
  w.tm_hour = h;
  w.tm_min = mn;
  w.tm_isdst = -1;
  return true;
}

bool raportyZablokowane(const StatusAB& s) { return s.rop == 1 || s.raport == "blocked"; }

bool modulPrzetwarza(const StatusAB& s) {
  return s.edf == "post-processing" || s.raport == "waiting_storage" || s.raport == "storage_busy";
}

int startZPrefiksu(int rop, const std::string& p, const std::string& dzis, int minuta) {
  if (rop != 1 || p.size() != 15 || p[8] != '_' || !dataOk(dzis) || minuta < 0) return -1;
  for (size_t i = 0; i < p.size(); i++)
    if (i != 8 && !isdigit((unsigned char)p[i])) return -1;
  std::string data = p.substr(0, 4) + "-" + p.substr(4, 2) + "-" + p.substr(6, 2);
  int hh = liczbaCalk(p.substr(9, 2).c_str()), mm = liczbaCalk(p.substr(11, 2).c_str()), ss = liczbaCalk(p.substr(13, 2).c_str());
  if (!dataOk(data) || dataZNr(nrDnia(data)) != data || hh > 23 || mm > 59 || ss > 59) return -1;
  int start = hh * 60 + mm;
  if (data == dzis) return start <= minuta ? start : -1;  // start z przyszłości: zły zegar
  if (data == dodajDni(dzis, -1)) return start;           // sesja od wczoraj wieczorem
  return -1;                                              // stara albo przyszła data
}

// ---------- odpowiedź HTTP ----------
bool cialoKompletne(int rozmiar, int wynikZapisu, size_t odebrane) {
  if (wynikZapisu < 0) return false;
  return rozmiar < 0 || (size_t)rozmiar == odebrane;
}

size_t limitOdpowiedziAB(const std::string& sciezka) {
  if (sciezka.rfind("/api/status", 0) == 0) return AB_MAKS_BAJTOW_STATUSU;
  if (sciezka == sciezkaListy()) return AB_MAKS_BAJTOW_LISTY;
  if (sciezka == sciezkaPobraniaStr()) return AB_MAKS_BAJTOW_STR;
  if (sciezka.rfind("/api/storage/download?path=", 0) == 0) return AB_MAKS_BAJTOW_PLIKU_SESJI;
  if (sciezka.rfind("/api/sessions", 0) == 0) return AB_MAKS_BAJTOW_SESJI;
  return AB_MAKS_BAJTOW_RAPORTU;
}

bool brakOdpowiedzi(int kod) { return kod != 200 && kod != 202 && kod != 409 && kod != 401; }

bool ponowicZadanie(int kod, int proba, long zostaloMs) {
  return brakOdpowiedzi(kod) && kod != AB_KOD_ZA_DUZO && proba < AB_PROB_ZADANIA &&
         zostaloMs >= (long)AB_ODSTEP_PROB_MS + (long)AB_LIMIT_ODPOWIEDZI_MS;
}

// ---------- job ----------
OdpowiedzAB klasyfikujOdpowiedz(int kod, const std::string& t) {
  OdpowiedzAB o;
  if (kod != 200 && kod != 202 && kod != 409) return o;
  static const FiltrJson F[] = {{"job", nullptr},        {"pending", nullptr},     {"days", TYLKO_KLUCZ},
                                {"present", TYLKO_KLUCZ}, {nullptr, nullptr}};
  Json j;
  // filtr: z dużego raportu zostają tylko dwa klucze, więc to tanie także przy 200; parser i tak sprawdza całość
  bool obiekt = parsujJson(t, j, F) && j.typ == Json::OBIEKT;
  if (obiekt) o.job = tekstPola(j["job"]);
  if (kod != 200) {  // 202, 409: w budowie, ciało może być puste (wtedy ponowny GET tej samej ścieżki)
    o.r = Odpowiedz::W_BUDOWIE;
    return o;
  }
  if (!obiekt) return o;  // 200 z ciałem urwanym albo nie-JSON: błąd tej próby (ponowienie), nie E-03
  bool pending = j["pending"].typ == Json::BOOL && j["pending"].b;
  // 200 z jobem, ale bez raportu (ani `days`, ani `present`), np. {"job":"edf:8","pending":false}: job skończony,
  // raport jeszcze nie w tej odpowiedzi → ponowny GET (W_BUDOWIE), nie raport bez kluczy (fałszywe E-03)
  bool bezRaportu = !j["days"].jest() && !j["present"].jest();
  o.r = !o.job.empty() && (pending || bezRaportu) ? Odpowiedz::W_BUDOWIE : Odpowiedz::GOTOWA;
  return o;
}

std::string sciezkaListy() { return "/api/report?source=edf&action=days"; }
std::string sciezkaNocy(long d) { return "/api/report?source=edf&day=" + std::to_string(d); }

std::string sciezkaJoba(const std::string& job) {
  std::string e;
  for (unsigned char c : job) {
    if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') e += (char)c;
    else {
      char b[4];
      snprintf(b, sizeof b, "%%%02X", c);
      e += b;
    }
  }
  return "/api/report?source=edf&job=" + e;
}

// ---------- lista nocy ----------
// Lista ma po jednym elemencie na dobę z karty modułu (setki, z czasem tysiące elementów, ok. 150 B każdy), więc nie
// budujemy drzewa Json (kilka małych alokacji na element w wewnętrznym RAM): czytnik idzie po tekście i dopisuje
// do `dni` tylko trzy pola; reszta elementu jest sprawdzana składniowo i pomijana. Jedna duża alokacja (wektor).
namespace {
// Pole elementu listy: liczba (`day`, `session_count`) albo bool (`stale`); inny typ = brak wartości.
bool liczbaPola(Czytnik& c, double& v, bool& jest) {
  c.ws();
  if (c.p < c.k && (*c.p == '-' || (*c.p >= '0' && *c.p <= '9'))) {
    jest = true;
    return c.liczba(&v);
  }
  jest = false;
  return c.wartosc(nullptr, nullptr);
}
bool elementListy(Czytnik& c, DzienListy& x) {
  c.ws();
  if (c.p >= c.k || *c.p != '{') return false;  // element musi być obiektem z liczbą `day`
  c.p++;
  bool maDay = false, byloDay = false, byloSes = false, byloStale = false;
  if (!c.znak('}')) {
    do {
      std::string klucz;  // klucze elementu są krótkie (bez alokacji, SSO)
      if (!c.tekst(&klucz) || !c.znak(':')) return false;
      double v;
      bool jest;
      if (klucz == "day" && !byloDay) {
        byloDay = true;
        if (!liczbaPola(c, v, jest)) return false;
        if (jest && v >= 0 && wInt(v)) { x.day = (long)v; maDay = true; }
      } else if (klucz == "session_count" && !byloSes) {
        byloSes = true;
        if (!liczbaPola(c, v, jest)) return false;
        if (jest && wInt(v)) x.sesje = (int)v;
      } else if (klucz == "stale" && !byloStale) {
        byloStale = true;
        c.ws();
        if (c.slowo("true")) x.stale = true;
        else if (c.slowo("false")) x.stale = false;
        else if (!c.wartosc(nullptr, nullptr)) return false;
      } else if (!c.wartosc(nullptr, nullptr)) {
        return false;
      }
    } while (c.znak(','));
    if (!c.znak('}')) return false;
  }
  return maDay;
}
}  // namespace

bool parsujListeNocy(const std::string& t, std::vector<DzienListy>& dni) {
  dni.clear();
  Czytnik c{t.data(), t.data() + t.size()};
  bool byloDays = false;
  auto zle = [&]() { dni.clear(); dni.shrink_to_fit(); return false; };
  if (!c.znak('{')) return zle();
  if (!c.znak('}')) {
    do {
      std::string klucz;
      if (!c.tekst(&klucz) || !c.znak(':')) return zle();
      if (klucz == "days" && !byloDays) {  // pierwsze wystąpienie klucza, jak Json::operator[]
        byloDays = true;
        if (!c.znak('[')) return zle();     // `days` musi być tablicą
        if (!c.znak(']')) {
          do {
            DzienListy x;
            if (!elementListy(c, x)) return zle();
            dni.push_back(x);
          } while (c.znak(','));
          if (!c.znak(']')) return zle();
        }
      } else if (!c.wartosc(nullptr, nullptr)) {
        return zle();
      }
    } while (c.znak(','));
    if (!c.znak('}')) return zle();
  }
  c.ws();
  if (c.p != c.k || !byloDays) return zle();
  return true;
}

std::string sciezkaSesjiStr() { return "/api/sessions?order=desc&limit=1"; }
std::string sciezkaPobraniaStr() { return "/api/storage/download?path=/STR.edf"; }

std::string dataPoranka(long day) { return day < 0 ? std::string() : dataZNr(day + 1); }

// ---------- raport nocy ----------
// [DO SPRAWDZENIA NA URZĄDZENIU] identyfikatory kanałów i jednostki: z Załącznika A i 4.11 spec (kod AirBridge
// v2.3.0, edf_report.cpp; tools/kalibracja/noce.py). Raport v2.2.1 ma 15 kanałów, a spec zna 12 identyfikatorów.
const std::vector<KanalAB> KANALY_AB = {
    {"Press", &Noc::cmed, &Noc::cp95, ZrodloKanalu::TYLKO_EDF},       // M-05, M-05p: w ASVAuto IPAP (4.11)
    {"EprPress", &Noc::emed, &Noc::ep95, ZrodloKanalu::EDF_ALBO_STR}, // M-06, M-06p: w ASVAuto EPAP; STR z dopiskiem
    {"Leak", &Noc::wmed, &Noc::wp95, ZrodloKanalu::DOWOLNE},          // M-07: L/min (L/s ×60, jak noce.py)
    // M-28, M-28p (wsparcie PS): spec nie zna kanału PS w raporcie, a mediany różnicy nie da się policzyć z median
    // Press i EprPress (R-DANE-32). [DO SPRAWDZENIA NA URZĄDZENIU]: czy któryś z 15 kanałów to PS; wtedy wpisać
    // jego id tutaj. Do tego czasu PS z AirBridge pusty (PS z karty zostaje w rekordzie przy zastąpieniu, wpiszNocAB).
    {"", &Noc::psmed, &Noc::psp95, ZrodloKanalu::TYLKO_EDF},
};

// OPCJE["MOP"] z tools/kalibracja/noce.py (VAR_CATALOG w settings_defs.h AirBridge)
const char* const TRYBY_MOP[] = {"CPAP", "AutoSet", "APAP", "S",     "ST",  "T",
                                 "VAuto", "ASV",    "ASVAuto", "iVAPS", "PAC", "AutoSet Her"};
const int N_TRYBOW_MOP = sizeof(TRYBY_MOP) / sizeof(TRYBY_MOP[0]);

static float wartoscKanalu(const Json& v) {
  double x;
  return v.liczba(x) ? (float)x : NAN;
}

double mnoznikWycieku(const std::string& j) {
  std::string t;
  for (unsigned char c : j)
    if (!isspace(c)) t += (char)tolower(c);
  if (t == "l/min") return 1;
  if (t == "l/s") return 60;
  return NAN;  // brak jednostki albo inna: brak danych, nie zgadujemy skali
}

static std::string liczbaKrotko(double v) {  // jak format(v, ".8g") w Pythonie
  char b[32];
  snprintf(b, sizeof b, "%.8g", v);
  return b;
}

// Liczba z tekstu: cała (`6.0`) albo z jednostką po spacji (`6.0 cmH2O`); false, gdy tekst nie zaczyna się liczbą
// albo po liczbie stoi coś innego niż spacje i jednostka z liter.
static bool liczbaZTekstu(const std::string& t, double& v) {
  if (t.empty()) return false;
  char* e;
  v = strtod(t.c_str(), &e);
  if (e == t.c_str()) return false;
  if (!(std::fabs(v) <= AB_MAKS_LICZBY)) return false;  // `inf`, `nan`, `1e400`
  if (*e == 0) return true;
  if (!isspace((unsigned char)*e)) return false;
  while (*e && isspace((unsigned char)*e)) e++;
  for (; *e; e++)
    if (!isalpha((unsigned char)*e) && !isdigit((unsigned char)*e) && *e != '/' && *e != '%') return false;
  return true;
}

static std::string trybZNumeru(double x) {
  if (x >= 0 && x == std::floor(x) && x < N_TRYBOW_MOP) return TRYBY_MOP[(int)x];
  return "";
}

static std::string wartoscUstawienia(const Json& f, bool mop) {
  const Json& v = f["value"];
  const Json& o = f["option"];
  double x;
  if (mop) {  // MOP: nazwa trybu; numer opcji (w `value` albo `option`) → nazwa jak OPCJE w noce.py
    if (v.typ == Json::TEKST && !v.s.empty() && !liczbaZTekstu(v.s, x)) return v.s;
    if (o.typ == Json::TEKST && !o.s.empty()) return o.s;
    if (v.liczba(x) || (v.typ == Json::TEKST && liczbaZTekstu(v.s, x)) || o.liczba(x)) return trybZNumeru(x);
    return "";
  }
  if (v.liczba(x)) return liczbaKrotko(x);
  if (v.typ != Json::TEKST) return "";
  if (liczbaZTekstu(v.s, x)) return liczbaKrotko(x);  // "6.0" → "6", "6.0 cmH2O" → "6"
  return v.s;
}

// Pierwszy wpis tagu w `settings.fields[]` jako krótka liczba; "" = brak tagu albo wartość nieczytelna (R-NOR-15).
static std::string ustawienieTekst(const Json& pola, const char* tag) {
  for (auto& f : pola.el)
    if (tekstPola(f["tag"]) == tag) {
      const Json& v = f["value"];
      double x;
      bool liczba = v.liczba(x) || (v.typ == Json::TEKST && liczbaZTekstu(v.s, x));
      return liczba ? wartoscUstawienia(f, false) : "";  // nieczytelna wartość = brak odczytu
    }
  return "";
}

// Tekst `Noc::ustawienia`: porównywany między nocami (zmiana ustawień R-NOR-07, karencje L-01, L-12, H-04). Format według
// trybu `MOP` (4.11, R-NOR-07.zmiana_ustawien w zestawie): `AutoSet/MPI/MPA/EPR`, `CPAP/IPC/EPR`, `ASV/EEP/MNS/MXS`;
// ASVAuto i tryby spoza wydania: `MOP/EAI/EAX/ANS/AXS` (jak TAGI_USTAWIEN w tools/prototyp/historia.py; historia
// w panelach używanych przed tym formatem ma te teksty zapisane, więc ich formatu nie zmieniamy). W trybie ze zmienianym formatem brak któregokolwiek
// tagu to brak odczytu (""), nie zmiana ustawień (R-NOR-15).
std::string ustawieniaZRaportu(const Json& s) {
  static const char* const TAGI[] = {"MOP", "EAI", "EAX", "ANS", "AXS"};  // jak TAGI_USTAWIEN w historia.py
  const Json& pola = s["fields"];
  if (s["available"].typ == Json::BOOL && !s["available"].b) return "";
  std::string w[5];
  bool jakis = false;
  for (auto& f : pola.el) {
    std::string tag = tekstPola(f["tag"]);
    for (int i = 0; i < 5; i++)
      if (tag == TAGI[i] && w[i].empty()) { w[i] = wartoscUstawienia(f, i == 0); jakis = jakis || !w[i].empty(); }
  }
  static const struct { const char* tryb; const char* tagi[3]; } WG_TRYBU[] = {
      {"AutoSet", {"MPI", "MPA", "EPR"}}, {"CPAP", {"IPC", "EPR", nullptr}}, {"ASV", {"EEP", "MNS", "MXS"}}};
  for (auto& t : WG_TRYBU) {
    if (w[0] != t.tryb) continue;
    std::string r = w[0];
    for (const char* tag : t.tagi) {
      if (!tag) break;
      std::string v = ustawienieTekst(pola, tag);
      if (v.empty()) return "";
      r += "/" + v;
    }
    return r;
  }
  if (!jakis) return "";
  return w[0] + "/" + w[1] + "/" + w[2] + "/" + w[3] + "/" + w[4];
}

// Tryb z tagu MOP; `jest` = tag obecny (wtedy nieznany numer opcji daje "" = tryb nieznany, bez zapasu ze statusu).
static std::string trybZRaportu(const Json& s, bool& jest) {
  jest = false;
  if (s["available"].typ == Json::BOOL && !s["available"].b) return "";
  for (auto& f : s["fields"].el)
    if (tekstPola(f["tag"]) == "MOP") { jest = true; return wartoscUstawienia(f, true); }
  return "";
}

// Liczba z tagu `tag` w `settings.fields[]` (`value` liczbą albo tekstem z liczbą, np. `6.0 cmH2O`); brak tagu,
// `available` = false albo wartość nieczytelna = NAN (R-AW-09). Pierwszy wpis tagu wygrywa (jak ustawieniaZRaportu()).
static float ustawienieLiczba(const Json& s, const char* tag) {
  if (s["available"].typ == Json::BOOL && !s["available"].b) return NAN;
  for (auto& f : s["fields"].el) {
    if (tekstPola(f["tag"]) != tag) continue;
    const Json& v = f["value"];
    double x;
    if (!(v.liczba(x) || (v.typ == Json::TEKST && liczbaZTekstu(v.s, x)))) return NAN;
    return std::isfinite(x) && x >= 0 ? (float)x : NAN;
  }
  return NAN;
}

void kanalyRaportu(Noc& n, const Json& kanaly, bool& kanalEdf) {
  kanalEdf = false;
  for (auto& k : KANALY_AB) {
    if (!*k.id) continue;
    bool zEdf = false;  // pierwszy kanał z EDF wygrywa z wartością ze STR
    for (auto& ch : kanaly.el) {
      if (tekstPola(ch["id"]) != k.id) continue;
      bool edf = tekstPola(ch["source"]) == "edf";
      if (edf && ch["median"].typ == Json::LICZBA) kanalEdf = true;
      if (k.zrodlo == ZrodloKanalu::TYLKO_EDF && !edf) continue;  // R-NOR-08: ciśnienie docelowe ze STR poza rekordem
      if (zEdf) break;
      if (!edf && !std::isnan(n.*k.med)) continue;  // drugi wpis STR nie nadpisuje pierwszego
      double skala = 1;
      if (k.med == &Noc::wmed) {
        skala = mnoznikWycieku(tekstPola(ch["unit"]));
        if (std::isnan(skala)) continue;  // jednostka wycieku nieznana albo brak: brak danych, nie zła liczba
      }
      n.*k.med = (float)(wartoscKanalu(ch["median"]) * skala);
      n.*k.p95 = (float)(wartoscKanalu(ch["p95"]) * skala);
      if (k.zrodlo == ZrodloKanalu::EDF_ALBO_STR) n.epapStr = !edf;
      zEdf = edf;
    }
  }
  if (n.epapStr && std::isnan(n.emed) && std::isnan(n.ep95)) n.epapStr = false;
  // M-13 (4.3): `median` kanału `RespRate` wprost; wpis z EDF wygrywa, bez EDF pierwszy z liczbą; `null`
  // albo zły typ = brak (R-DANE-07, R-AW-22)
  bool respEdf = false;
  for (auto& ch : kanaly.el) {
    if (tekstPola(ch["id"]) != "RespRate" || ch["median"].typ != Json::LICZBA) continue;
    bool edf = tekstPola(ch["source"]) == "edf";
    if (respEdf || (!edf && !std::isnan(n.m13))) continue;
    if (!std::isfinite(ch["median"].n)) continue;
    n.m13 = (float)ch["median"].n;
    respEdf = edf;
  }
  // M-10 (4.3): kanał `FlowLim`, `median` i `p95` wprost (skala 0–1); wpis z EDF wygrywa; brak = NAN (R-AW-22)
  bool flEdf = false;
  for (auto& ch : kanaly.el) {
    if (tekstPola(ch["id"]) != "FlowLim") continue;
    bool edf = tekstPola(ch["source"]) == "edf";
    if (flEdf || (!edf && (!std::isnan(n.m10) || !std::isnan(n.m10p)))) continue;
    double x;
    n.m10 = ch["median"].liczba(x) && std::isfinite(x) && x >= 0 ? (float)x : NAN;
    n.m10p = ch["p95"].liczba(x) && std::isfinite(x) && x >= 0 ? (float)x : NAN;
    flEdf = edf;
  }
}

std::string ustawieniaRaportu(Noc& n, const Json& ust, bool& maMop) {
  n.ustawienia = czystePole(ustawieniaZRaportu(ust));
  // ustawienia jako liczby, tylko do odczytu w regułach (ADR-0002: nigdy jako wartość docelowa na ekranie)
  n.mpa = ustawienieLiczba(ust, "MPA");
  n.mpi = ustawienieLiczba(ust, "MPI");
  n.ipc = ustawienieLiczba(ust, "IPC");
  n.eep = ustawienieLiczba(ust, "EEP");
  n.mns = ustawienieLiczba(ust, "MNS");
  n.mxs = ustawienieLiczba(ust, "MXS");
  n.eai = ustawienieLiczba(ust, "EAI");  // ASVAuto (4.11)
  n.eax = ustawienieLiczba(ust, "EAX");
  n.ans = ustawienieLiczba(ust, "ANS");
  n.axs = ustawienieLiczba(ust, "AXS");
  n.epr = ustawienieLiczba(ust, "EPR");  // CPAP, AutoSet (M-21); w ASV i ASVAuto pomijany przez reguły (4.11) // bez `;` i znaków sterujących
  return trybZRaportu(ust, maMop);
}

NocAB nocZRaportu(const std::string& t, long dzien, const std::string& trybZapasowy,
                  const std::string& pna, const std::string& srn) {
  static const FiltrJson KANAL[] = {{"id", nullptr},  {"unit", nullptr},   {"median", nullptr},
                                    {"p95", nullptr}, {"source", nullptr}, {nullptr, nullptr}};
  static const FiltrJson POLE[] = {{"tag", nullptr}, {"value", nullptr}, {"option", nullptr}, {nullptr, nullptr}};
  static const FiltrJson USTAW[] = {{"available", nullptr}, {"fields", POLE}, {nullptr, nullptr}};
  static const FiltrJson SESJA[] = {{"start_ms", nullptr}, {"end_ms", nullptr}, {nullptr, nullptr}};  // M-17, M-18
  static const FiltrJson ZDARZ[] = {{"oa", nullptr}, {"ca", nullptr}, {"h", nullptr}, {"ua", nullptr}, {nullptr, nullptr}};
  static const FiltrJson F[] = {{"day", nullptr},          {"present", nullptr},       {"partial", nullptr},
                                {"used_seconds", nullptr}, {"ahi", nullptr},           {"events_known", nullptr},
                                {"session_count", nullptr}, {"channels", KANAL},       {"settings", USTAW},
                                {"counts", ZDARZ},         {"csr_seconds", nullptr},   {"leak_high_seconds", nullptr},
                                {"sessions", SESJA},       {nullptr, nullptr}};
  NocAB w;
  Json j;
  if (!parsujJson(t, j, F)) return w;  // NIECZYTELNY: urwany albo nie-JSON (ponowienie, nie E-03)
  if (j.typ != Json::OBIEKT) {         // poprawny JSON, ale nie raport: zły format (R-AW-02)
    w.stan = StanRaportu::ZLY_FORMAT;
    return w;
  }
  double v;
  // `day` w raporcie inny niż żądany: to nie jest raport tej doby, nic nie zapisujemy (NIECZYTELNY, ponowienie).
  // dzien < 0 (tylko testy): numer dnia z raportu.
  if (j["day"].liczba(v)) {
    if (!wInt(v)) return w;  // numer doby spoza zakresu: nie raport tej doby
    if (dzien >= 0 && (long)v != dzien) return w;
    if (dzien < 0) dzien = (long)v;
  }
  if (dzien < 0) return w;
  // klucze krytyczne (Załącznik A, R-AW-02): obecne, wartość null albo właściwy typ
  auto typOk = [](const Json& x, Json::Typ oczek) { return x.typ == oczek || x.typ == Json::NUL; };
  const Json &present = j["present"], &partial = j["partial"], &used = j["used_seconds"], &ahi = j["ahi"],
             &ev = j["events_known"];
  if (!typOk(present, Json::BOOL) || !typOk(partial, Json::BOOL) || !typOk(used, Json::LICZBA) ||
      !typOk(ahi, Json::LICZBA) || !typOk(ev, Json::BOOL)) {
    w.stan = StanRaportu::ZLY_FORMAT;
    return w;
  }
  double s;
  if (j["session_count"].liczba(s) && wInt(s)) w.sesje = (int)s;
  Noc& n = w.noc;
  // M-17 (4.3): bez `sessions[]` (fail-path R-DANE-27) `session_count` − 1, najmniej 0; brak, null, zły typ, liczba
  // ujemna albo niecałkowita = brak (-1)
  if (j["session_count"].liczba(s) && s >= 0 && s < 1000000 && s == std::floor(s)) n.m17 = s >= 1 ? (int)s - 1 : 0;
  // M-17 i M-18 według R-DANE-27 z `sessions[]`: `start_ms` i `end_ms` to sekundy kalendarza cywilnego aparatu × 1000
  // (AirBridge koduje czas lokalny aparatu jak UTC, bez strefy), więc minuty od 12:00 dnia `day` liczone wprost, bez
  // przeliczania na strefę panelu (ta sama skala co noc z plików sesji, src/noc_pliki.h)
  {
    std::vector<SesjaCywilna> ses;
    for (auto& se : j["sessions"].el) {
      SesjaCywilna x;
      double a, b;
      if (se["start_ms"].liczba(a) && a > 0 && a < AB_MAKS_LICZBY) {
        x.start = true;
        x.startS = (double)(long long)a / 1000.0;
        if (se["end_ms"].liczba(b) && b >= a && b < AB_MAKS_LICZBY) {
          x.koniec = true;
          x.koniecS = (double)(long long)b / 1000.0;
        }
      }
      ses.push_back(x);
    }
    if (!ses.empty()) {
      int m17 = -1;
      double m18 = NAN;
      m17m18Sesji(ses, dzien, R_DANE_27_START_NOCY_MIN, R_DANE_27_WCZESNY_SEN_S, m17, m18);
      n.m17 = m17;
      n.m18 = !std::isnan(m18) && m18 >= 0 && m18 < 1440 ? (int)std::floor(m18) : -1;
    }
  }
  // model i numer aparatu z `/api/status` tego samego wybudzenia (R-NOR-25)
  n.pna = czystePole(pna);
  n.srn = czystePole(srn);
  n.data = dataPoranka(dzien);
  n.zrodlo = 'A';
  // partial null traktujemy jak „w budowie”: noc „częściowa” (nieważna) do nadpisania kompletnym raportem
  n.czesciowa = partial.typ != Json::BOOL || partial.b;
  // jak historia.py: round(s / 60); więcej niż doba (86 400 s) to nie czas terapii jednej doby: brak danych
  n.m01 = used.typ == Json::LICZBA && used.n >= 0 && used.n <= 86400 ? (int)lround(used.n / 60.0) : -1;
  n.ahi = ahi.typ == Json::LICZBA ? (float)ahi.n : NAN;
  bool kanalEdf = false;
  kanalyRaportu(n, j["channels"], kanalEdf);
  // M-03 (na godzinę) i M-09 (minuty); brak pola albo zły typ = brak danych tej metryki (R-AW-22)
  const Json& cnt = j["counts"];
  if (used.typ == Json::LICZBA && used.n > 0) {
    double c;
    // bardzo małe `used_seconds` daje wynik poza float: brak danych (naFloat()), nie `inf` w pliku
    if (cnt["oa"].liczba(c) && c >= 0) n.oa = naFloat(c * 3600.0 / used.n);
    if (cnt["ca"].liczba(c) && c >= 0) n.ca = naFloat(c * 3600.0 / used.n);
    if (cnt["h"].liczba(c) && c >= 0) n.h = naFloat(c * 3600.0 / used.n);
    if (cnt["ua"].liczba(c) && c >= 0) n.ua = naFloat(c * 3600.0 / used.n);  // UA/h (suma OA w AutoSet)
  }
  if (j["csr_seconds"].liczba(v) && v >= 0) n.csr = (float)(v / 60.0);
  // M-08 (4.3, R-DANE-23): minuty dużego wycieku (> 24 L/min, próg AirBridge `leak_threshold`); brak pola,
  // null albo zły typ = brak tej metryki (R-AW-22: „–”, bez E-03)
  if (j["leak_high_seconds"].liczba(v) && v >= 0) n.m08 = (float)(v / 60.0);
  bool maMop;
  w.tryb = ustawieniaRaportu(n, j["settings"], maMop);
  if (!maMop) w.tryb = trybZapasowy;  // bez tagu MOP tryb z /api/status; MOP nieczytelny = tryb nieznany (R-DANE-29)
  // 4.2 (R-DANE-13) i tryb obsługiwany (R-DANE-29): noc w innym albo nieznanym trybie zapisana, ale nie ważna.
  // Próg 4 h z tej samej zaokrąglonej wartości co M-01, seria (M-20) i kamienie (S-K-28): 14 399,6 s = 240 min = 4 h.
  bool ponad4h = n.m01 >= 240;
  n.wazna = !n.czesciowa && present.typ == Json::BOOL && present.b && ev.typ == Json::BOOL && ev.b && ponad4h &&
            trybObslugiwany(w.tryb);  // ADR-0030: cztery tryby; inny albo nieznany: R-DANE-29. Model (`pna`) nie decyduje
  n.tryb = czystePole(w.tryb);  // przeglądu tryb nocy w rekordzie ("" = nieznany)
  w.pelna = !n.czesciowa && ev.typ == Json::BOOL && ev.b && kanalEdf;
  w.stan = n.czesciowa ? StanRaportu::CZESCIOWA : StanRaportu::KOMPLETNA;
  return w;
}

// ---------- R-DANE-27 ----------
void m17m18Sesji(const std::vector<SesjaCywilna>& sesje, long dzien, double startNocyMin, double wczesnySenS, int& m17,
                 double& m18) {
  m18 = NAN;
  if (sesje.empty()) {
    m17 = 0;
    return;
  }
  struct S {
    bool start;
    double st;
    bool dl;
    double d;
  };
  double poludnie = (double)dzien * 86400.0 + 12 * 3600.0;
  std::vector<S> v;
  v.reserve(sesje.size());
  for (auto& s : sesje) {
    if (!s.start) {
      v.push_back({false, 0, false, 0});
      continue;
    }
    v.push_back({true, (s.startS - poludnie) / 60.0, s.koniec, s.koniec ? s.koniecS - s.startS : 0});
  }
  std::vector<S> po18;
  for (auto& x : v)
    if (!x.start || x.st >= startNocyMin) po18.push_back(x);
  bool maDluga = false;
  for (auto& x : po18) maDluga = maDluga || (x.start && x.dl && x.d >= wczesnySenS);
  if (!maDluga) {  // wyjątek wczesnego snu: najdłuższa sesja przed 18:00 co najmniej 4 h
    const S* najdl = nullptr;
    for (auto& x : v)
      if (x.start && x.st < startNocyMin && x.dl && (!najdl || x.d > najdl->d)) najdl = &x;
    if (najdl && najdl->d >= wczesnySenS) {
      double od = najdl->st;
      po18.clear();
      for (auto& x : v)
        if (!x.start || x.st >= od) po18.push_back(x);
    }
  }
  m17 = std::max(0, (int)po18.size() - 1);
  std::vector<S> ze;
  for (auto& x : po18)
    if (x.start) ze.push_back(x);
  if (ze.empty()) return;
  bool wszystkie = true;
  for (auto& x : ze) wszystkie = wszystkie && x.dl;
  const S* wyb = &ze[0];
  for (auto& x : ze) {
    if (wszystkie) {
      if (x.d > wyb->d || (x.d == wyb->d && x.st < wyb->st)) wyb = &x;  // najdłuższa, przy remisie wcześniejsza
    } else if (x.st < wyb->st) {
      wyb = &x;  // bez długości: najwcześniejsza
    }
  }
  m18 = wyb->st;
}

// ---------- próby dób ----------
// Pole wiersza prób: same cyfry dziesiętne (bez znaku, spacji i reszty) o wartości ≤ maks; inaczej false.
static bool poleProby(const std::string& t, unsigned long long maks, unsigned long long& v) {
  if (t.empty() || t.size() > 20) return false;
  for (char c : t)
    if (c < '0' || c > '9') return false;
  errno = 0;
  char* e = nullptr;
  v = strtoull(t.c_str(), &e, 10);
  return errno != ERANGE && *e == 0 && v <= maks;
}

ProbyDob parsujProby(const std::vector<std::string>& wiersze) {
  ProbyDob p;
  for (auto& l : wiersze) {  // day;proby;ostatnia;stale(0/1), bez sscanf
    std::vector<std::string> f;
    size_t od = 0;
    for (size_t k; (k = l.find(';', od)) != std::string::npos; od = k + 1) f.push_back(l.substr(od, k - od));
    f.push_back(l.substr(od));
    unsigned long long d, n, t, st;
    if (f.size() != 4 || !poleProby(f[0], LONG_MAX, d) || !poleProby(f[1], INT_MAX, n) ||
        !poleProby(f[2], UINT32_MAX, t) || !poleProby(f[3], 1, st))
      continue;
    p[(long)d] = ProbyDoby{(int)n, (uint32_t)t, st == 1};
  }
  return p;
}

std::vector<std::string> wierszeProb(const ProbyDob& p) {
  std::vector<std::string> w;
  for (auto& x : p) {
    if (x.second.proby == 0 && !x.second.staleZrobione) continue;
    w.push_back(std::to_string(x.first) + ";" + std::to_string(x.second.proby) + ";" +
                std::to_string((unsigned long)x.second.ostatnia) + ";" + (x.second.staleZrobione ? "1" : "0"));
  }
  return w;
}

void zapiszNieudanaProbe(ProbyDob& p, long day, uint32_t teraz) {
  ProbyDoby& x = p[day];
  x.proby++;
  x.ostatnia = teraz;
}

bool dobaWyczerpana(const ProbyDob& p, long day) {
  auto i = p.find(day);
  return i != p.end() && i->second.proby >= AB_PROB_DOBY;
}

bool dobaCzeka(const ProbyDob& p, long day, uint32_t teraz) {
  auto i = p.find(day);
  if (i == p.end() || i->second.proby == 0 || i->second.proby >= AB_PROB_DOBY) return false;
  return teraz < i->second.ostatnia + AB_ODSTEP_DOBY_S;
}

bool odswiezStale(ProbyDob& p, const std::vector<DzienListy>& lista) {
  bool zm = false;
  for (auto& x : lista) {
    auto i = p.find(x.day);
    if (!x.stale && i != p.end() && i->second.staleZrobione) {
      i->second.staleZrobione = false;
      if (i->second.proby == 0) p.erase(i);
      zm = true;
    }
  }
  return zm;
}

// ---------- plan ----------
bool sprawdzenie11(const std::string& dzis, int minuta, const std::string& rano, const std::string& spr) {
  return dataOk(dzis) && minuta >= 11 * 60 && !slotNocny(minuta) && rano == dzis && spr != dzis;
}

static bool kompletnaZAirBridge(const std::vector<Noc>& hist, const std::string& data) {
  const Noc* n = nocZDnia(hist, data);
  return n && n->zrodlo == 'A' && !n->czesciowa;
}

bool potrzebnaLista(const std::vector<Noc>& hist, const std::string& dzis, int minuta, bool spr11, int gotowe,
                    uint32_t czekajDo, uint32_t teraz, const std::set<long>& pominiete) {
  if (!dataOk(dzis) || slotNocny(minuta)) return false;
  bool ostatniaJest = kompletnaZAirBridge(hist, dzis) || pominiete.count(nrDnia(dzis) - 1);
  if (!ostatniaJest || spr11 || gotowe != 0) return true;
  return czekajDo != 0 && teraz >= czekajDo;  // tylko czekające doby: lista raz, po końcu czekania
}

std::vector<long> planPobrania(const std::vector<DzienListy>& lista, const std::vector<Noc>& hist,
                               const std::set<long>& pominiete, const std::string& dzis, bool spr11, int maks,
                               const ProbyDob* proby, uint32_t teraz) {
  std::vector<long> w;
  if (!dataOk(dzis)) return w;
  std::vector<DzienListy> dni = lista;
  std::sort(dni.begin(), dni.end(), [](const DzienListy& a, const DzienListy& b) { return a.day > b.day; });
  long poprzedni = -2;
  for (auto& x : dni) {
    if ((int)w.size() >= maks) break;
    if (x.day == poprzedni) continue;  // ten sam dzień dwa razy na liście
    poprzedni = x.day;
    std::string d = dataPoranka(x.day);
    if (d > dzis) continue;  // noc z przyszłości (zły zegar modułu albo panelu)
    bool ostatnia = d == dzis;
    // limit prób i czekanie po nieudanej próbie obowiązują każdą starszą dobę, także oznaczoną `stale`
    bool czeka = proby && !ostatnia && (dobaWyczerpana(*proby, x.day) || dobaCzeka(*proby, x.day, teraz));
    if (proby && x.stale) {  // raport doby nieaktualny według modułu: pobrać raz jeszcze (R-AW-03 rozstrzyga zapis)
      auto i = proby->find(x.day);
      if (i == proby->end() || !i->second.staleZrobione) {
        if (!czeka) w.push_back(x.day);
        continue;
      }
    }
    bool ponowne = spr11 && ostatnia;
    if (kompletnaZAirBridge(hist, d) && !ponowne) continue;
    // noc ze STR aparatu (ADR-0035) na starszej dobie, dla której lista nie podaje sesji (`session_count` 0 albo brak):
    // AirBridge zna tę dobę tylko ze STR, więc raport byłby niepełny i nie wyparłby nocy 'S' (zbędne pobranie);
    // doba z sesjami jest pobierana, a pełny raport z EDF zastępuje noc 'S'. Ostatnia noc zawsze (do „pominiętych”
    // po niepełnym raporcie, R-DANE-18)
    if (!ostatnia) {
      const Noc* h = nocZDnia(hist, d);
      if (h && h->zrodlo == 'S' && x.sesje < 1) continue;
    }
    if (pominiete.count(x.day) && !ponowne) continue;
    if (czeka) continue;
    w.push_back(x.day);
  }
  return w;
}

Zalegle policzZalegle(const std::vector<DzienListy>& lista, const std::vector<Noc>& hist,
                      const std::set<long>& pominiete, const std::string& dzis, const ProbyDob& proby, uint32_t teraz) {
  Zalegle z;
  if (!dataOk(dzis)) return z;
  for (long d : planPobrania(lista, hist, pominiete, dzis, false, 1 << 30, &proby, teraz))
    if (dataPoranka(d) != dzis) z.gotowe++;
  for (auto& x : lista) {
    std::string d = dataPoranka(x.day);
    if (d >= dzis || !dobaCzeka(proby, x.day, teraz)) continue;
    auto i = proby.find(x.day);
    bool staleDo = x.stale && (i == proby.end() || !i->second.staleZrobione);
    if ((kompletnaZAirBridge(hist, d) || pominiete.count(x.day)) && !staleDo) continue;
    z.czekajace++;
    uint32_t koniec = proby.at(x.day).ostatnia + AB_ODSTEP_DOBY_S;
    if (!z.czekajDo || koniec < z.czekajDo) z.czekajDo = koniec;
  }
  return z;
}

PoRaporcie poRaporcie(StanRaportu stan, const std::string& data, const std::string& dzis, int minuta, int proby) {
  bool ostatnia = data == dzis;
  bool wyczerpane = proby >= AB_PROB_DOBY;
  switch (stan) {
    case StanRaportu::KOMPLETNA: return PoRaporcie::ZAPISZ;
    case StanRaportu::CZESCIOWA: return ostatnia && minuta < 11 * 60 ? PoRaporcie::CZEKAJ : PoRaporcie::ZAPISZ;
    case StanRaportu::ZLY_FORMAT: return ostatnia ? PoRaporcie::E03 : wyczerpane ? PoRaporcie::POMIN : PoRaporcie::PONOW;
    default: return wyczerpane ? (ostatnia ? PoRaporcie::E03 : PoRaporcie::POMIN) : PoRaporcie::PONOW;
  }
}

std::set<long> parsujPominiete(const std::vector<std::string>& wiersze) {
  std::set<long> p;
  for (auto& l : wiersze) {
    char* e;
    long v = strtol(l.c_str(), &e, 10);
    if (!l.empty() && *e == 0 && v >= 0) p.insert(v);
  }
  return p;
}

std::vector<std::string> wierszePominietych(const std::set<long>& p) {
  std::vector<std::string> w;
  w.reserve(p.size());
  for (long v : p) w.push_back(std::to_string(v));
  return w;
}

bool rekordPelny(const Noc& r) {
  return !r.czesciowa && (r.zrodlo == 'K' || !std::isnan(epapMed(r)) || !std::isnan(r.cmed));
}

bool wpiszNocAB(std::vector<Noc>& hist, const Noc& n, bool nadpisz, bool pelna, bool* zostawiony) {
  if (zostawiony) *zostawiony = false;
  if (!dataOk(n.data)) return false;
  for (auto& h : hist) {
    if (h.data != n.data) continue;
    if (n.czesciowa) {  // R-AW-03: noc „częściowa” nie wypiera kompletnego rekordu
      if (!(h.zrodlo == 'A' && h.czesciowa)) {
        if (zostawiony) *zostawiony = true;
        return false;
      }
      Noc c = n;
      if (c.pna.empty()) c.pna = h.pna;  // brak odczytu aparatu nie jest zmianą (R-NOR-25)
      if (c.srn.empty()) c.srn = h.srn;
      if (trescNocy(h) == trescNocy(c)) return false;
      h = c;
      return true;
    }
    // doba niepełna (np. tylko STR) nie wypiera pełniejszego rekordu ani nocy ze STR aparatu (ADR-0035 pkt 4: noc
    // ze STR ustępuje tylko pełnej nocy z AirBridge i nocy z karty)
    if (!pelna && (rekordPelny(h) || h.zrodlo == 'S')) {
      if (zostawiony) *zostawiony = true;
      return false;
    }
    if (h.zrodlo == 'A' && !h.czesciowa && !nadpisz) return false;
    Noc m = n;
    // tylko z karty: z rekordu karty każde pole, którego raport nie ma; z wcześniejszego rekordu A tylko
    // pola, których raport AirBridge nigdy nie podaje (PS, KANALY_AB bez id), więc mogły przyjść wyłącznie z karty
    uzupelnijNoc(m, h, h.zrodlo == 'K' ? PolaUzupelnienia::WSZYSTKIE : PolaUzupelnienia::TYLKO_SPOZA_AIRBRIDGE);
    // brak odczytu modelu albo numeru w `/api/status` tego wybudzenia nie jest
    // zmianą aparatu (R-NOR-25), więc ponowne pobranie (R-DANE-24) zostawia odczyt z poprzedniego rekordu
    if (m.pna.empty()) m.pna = h.pna;
    if (m.srn.empty()) m.srn = h.srn;
    if (trescNocy(m) == trescNocy(h)) return false;
    h = m;
    return true;
  }
  hist.push_back(n);
  return true;
}

WynikImportu wpiszImport(std::vector<Noc>& hist, const Noc& k) {
  for (auto& h : hist) {
    if (h.data != k.data) continue;
    if (rekordPelny(h)) return uzupelnijNoc(h, k) ? WynikImportu::UZUPELNIONA : WynikImportu::POMINIETA;
    Noc m = k;
    uzupelnijNoc(m, h);  // M-03 i M-09 z AirBridge, których karta nie ma
    h = m;
    return WynikImportu::ZASTAPIONA;
  }
  hist.push_back(k);
  return WynikImportu::DODANA;
}
