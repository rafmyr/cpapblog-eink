// SPDX-License-Identifier: GPL-3.0-or-later
#include "otoczenie.h"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include "airbridge.h"  // parser JSON z filtrem (ten sam co dla AirBridge)
#include "ekrany.h"     // znakJestWKroju: opis i miasto tylko ze znakami, które panel umie narysować
#include "wspolne.h"

void wyczyscOtoczenie(Otoczenie& o, bool skonfigurowane, uint32_t adres) {
  memset(&o, 0, sizeof o);
  o.magia = OTO_MAGIA;
  o.adres = adres;
  o.stan = (int8_t)(skonfigurowane ? StanOto::NIE_BYLO : StanOto::WYLACZONE);
  o.wersja = -1;
  o.temp = NAN;
  o.pm25 = -1;
  o.co2 = -1;
  o.tempSyp = o.wilgSyp = NAN;
}

uint32_t hashAdresu(const char* url) {
  uint32_t h = 2166136261u;
  for (const char* c = url; c && *c; c++) h = (h ^ (uint8_t)*c) * 16777619u;
  return h ? h : 1;
}

int probOtoczenia(bool poprzedniaNieudana, bool slotNocny) { return poprzedniaNieudana || slotNocny ? 1 : OTO_PROB; }

// ---------- czas ----------
static bool cyfry(const std::string& s, size_t i, size_t n, int& v) {
  if (i + n > s.size()) return false;
  v = 0;
  for (size_t k = i; k < i + n; k++) {
    if (s[k] < '0' || s[k] > '9') return false;
    v = v * 10 + (s[k] - '0');
  }
  return true;
}

bool epochZIso(const std::string& s, uint32_t& ep) {
  int hh, mi, ss = 0;
  if (s.size() < 17) return false;
  std::string d = s.substr(0, 10);
  if (!dataOk(d) || dataZNr(nrDnia(d)) != d) return false;  // także 31.02
  if (s[10] != 'T' && s[10] != ' ') return false;
  if (!cyfry(s, 11, 2, hh) || s[13] != ':' || !cyfry(s, 14, 2, mi)) return false;
  size_t i = 16;
  if (i < s.size() && s[i] == ':') {
    if (!cyfry(s, i + 1, 2, ss)) return false;
    i += 3;
    if (i < s.size() && s[i] == '.') {  // ułamek sekundy: pomijany
      i++;
      size_t p = i;
      while (i < s.size() && s[i] >= '0' && s[i] <= '9') i++;
      if (i == p) return false;
    }
  }
  if (hh > 23 || mi > 59 || ss > 59) return false;
  long off = 0;
  if (i < s.size() && (s[i] == 'Z' || s[i] == 'z')) {
    i++;
  } else if (i < s.size() && (s[i] == '+' || s[i] == '-')) {
    int oh, om;
    bool dwukropek = i + 3 < s.size() && s[i + 3] == ':';  // `+01:00` albo `+0100`
    if (!cyfry(s, i + 1, 2, oh) || !cyfry(s, i + (dwukropek ? 4 : 3), 2, om)) return false;
    if (oh > 14 || om > 59) return false;
    off = (oh * 60L + om) * 60L * (s[i] == '-' ? -1 : 1);
    i += dwukropek ? 6 : 5;
  } else {
    return false;  // bez strefy czas jest niejednoznaczny
  }
  if (i != s.size()) return false;
  long long e = (long long)nrDnia(d) * 86400LL + hh * 3600LL + mi * 60LL + ss - off;
  if (e <= 0 || e > 0xFFFFFFFFLL) return false;
  ep = (uint32_t)e;
  return true;
}

bool swiezy(uint32_t ts, uint32_t teraz, uint32_t maxS) {
  if (!ts || teraz < 1700000000u) return false;  // bez zegara panel nie ocenia świeżości
  if (ts > teraz) return ts - teraz <= OTO_ZAPAS_ZEGARA_S;
  return teraz - ts <= maxS;
}

// ---------- parser kontraktu ----------
namespace {
const FiltrJson F_POGODA[] = {{"temp", nullptr}, {"opis", nullptr}, {"miasto", nullptr}, {"ts", nullptr},
                              {nullptr, nullptr}};
const FiltrJson F_POWIETRZE[] = {{"pm25", nullptr}, {"ts", nullptr}, {nullptr, nullptr}};
const FiltrJson F_SYPIALNIA[] = {{"co2", nullptr}, {"temp", nullptr}, {"wilg", nullptr}, {"ts", nullptr},
                                 {nullptr, nullptr}};
const FiltrJson F_OSTRZEZENIE[] = {{"kod", nullptr}, {"od", nullptr}, {nullptr, nullptr}};
const FiltrJson F_OTOCZENIE[] = {{"wersja", nullptr},   {"pogoda", F_POGODA},       {"powietrze", F_POWIETRZE},
                                 {"sypialnia", F_SYPIALNIA}, {"ostrzezenia", F_OSTRZEZENIE}, {nullptr, nullptr}};

// liczba albo NAN (null, brak, inny typ, poza zakresem): nigdy 0 w miejscu braku
double liczbaLubBrak(const Json& j, double min, double max) {
  double v;
  if (!j.liczba(v) || std::isnan(v) || v < min || v > max) return NAN;
  return v;
}

// Tekst do wyświetlenia: bez znaków sterujących i znaków spoza kroju panelu (pominięte), urwana albo błędna sekwencja
// UTF-8 kończy tekst; najwyżej `maks` znaków (punktów kodowych).
std::string tekstDoEkranu(const Json& j, size_t maks) {
  if (j.typ != Json::TEKST) return "";
  std::string w;
  size_t n = 0;
  const std::string& t = j.s;
  for (size_t i = 0; i < t.size() && n < maks;) {
    unsigned char c = (unsigned char)t[i];
    size_t dl = c < 0x80 ? 1 : (c & 0xE0) == 0xC0 ? 2 : (c & 0xF0) == 0xE0 ? 3 : (c & 0xF8) == 0xF0 ? 4 : 0;
    if (!dl || i + dl > t.size()) break;  // urwane albo bajt kontynuacji na początku: koniec
    uint32_t cp = dl == 1 ? c : c & (0x7F >> dl);
    bool ok = true;
    for (size_t k = 1; k < dl; k++) {
      unsigned char d = (unsigned char)t[i + k];
      if ((d & 0xC0) != 0x80) { ok = false; break; }
      cp = (cp << 6) | (d & 0x3F);
    }
    if (!ok) break;
    if (znakJestWKroju(cp) && cp != 0xFEFF) {
      w += t.substr(i, dl);
      n++;
    }
    i += dl;
  }
  while (!w.empty() && w.back() == ' ') w.pop_back();
  size_t p = w.find_first_not_of(' ');
  return p == std::string::npos ? std::string() : w.substr(p);
}

// `ts` bloku: brak pola albo null = chwila odebrania odpowiedzi (źródło ocenia świeżość przy wydaniu); tekst
// nieczytelny, bez strefy albo inny typ = false
bool tsBloku(const Json& b, uint32_t teraz, uint32_t& ts) {
  const Json& t = b["ts"];
  if (!t.jest() || t.typ == Json::NUL) { ts = teraz; return true; }
  return t.typ == Json::TEKST && epochZIso(t.s, ts);
}

void kopiuj(char* cel, size_t n, const std::string& z) {
  size_t k = z.size() < n - 1 ? z.size() : n - 1;
  // nie tniemy w środku znaku UTF-8
  while (k > 0 && k < z.size() && ((unsigned char)z[k] & 0xC0) == 0x80) k--;
  memcpy(cel, z.data(), k);
  cel[k] = 0;
}
}  // namespace

StanOto parsujOtoczenie(const std::string& t, uint32_t teraz, Otoczenie& o) {
  o.proba = teraz;
  o.kod = 200;
  if (t.size() > OTO_MAX_BAJTOW) { o.stan = (int8_t)StanOto::ZA_DUZE; return StanOto::ZA_DUZE; }
  Json j;
  bool bom = t.size() >= 3 && (unsigned char)t[0] == 0xEF && (unsigned char)t[1] == 0xBB && (unsigned char)t[2] == 0xBF;
  if (!parsujJson(bom ? t.substr(3) : t, j, F_OTOCZENIE) || j.typ != Json::OBIEKT) {
    o.stan = (int8_t)StanOto::NIECZYTELNE;
    return StanOto::NIECZYTELNE;
  }
  double w;
  if (!j["wersja"].liczba(w) || w != (double)OTO_WERSJA) {
    o.wersja = j["wersja"].liczba(w) && w > -1000 && w < 1000 ? (int16_t)w : -1;
    // źródło mówi innym językiem: jak brak źródła, więc bez starych danych (nieudana próba i nieczytelne ciało je
    // zostawiają, z ich czasem)
    o.pogoda = o.powietrze = o.sypialnia = o.co2Milczy = false;
    o.stan = (int8_t)StanOto::ZLA_WERSJA;
    return StanOto::ZLA_WERSJA;
  }
  o.wersja = OTO_WERSJA;
  o.zleTs = 0;
  // pogoda: pokazujemy ją tylko z temperaturą; opis i miasto opcjonalne
  const Json& p = j["pogoda"];
  o.pogoda = false;
  o.temp = NAN;
  o.opis[0] = o.miasto[0] = 0;
  if (p.typ == Json::OBIEKT) {
    o.temp = (float)liczbaLubBrak(p["temp"], -60, 60);
    kopiuj(o.opis, sizeof o.opis, tekstDoEkranu(p["opis"], 20));
    kopiuj(o.miasto, sizeof o.miasto, tekstDoEkranu(p["miasto"], 32));
    bool tsOk = tsBloku(p, teraz, o.tsPogody);
    if (!tsOk) o.zleTs |= 1;
    o.pogoda = tsOk && !std::isnan(o.temp);
  }
  const Json& a = j["powietrze"];
  o.powietrze = false;
  o.pm25 = -1;
  if (a.typ == Json::OBIEKT) {
    double v = liczbaLubBrak(a["pm25"], 0, OTO_PM25_MAX);
    o.pm25 = std::isnan(v) ? -1 : (int16_t)std::lround(v);
    bool tsOk = tsBloku(a, teraz, o.tsPowietrza);
    if (!tsOk) o.zleTs |= 2;
    o.powietrze = tsOk && o.pm25 >= 0;
  }
  const Json& s = j["sypialnia"];
  o.sypialnia = false;
  o.co2 = -1;
  o.tempSyp = o.wilgSyp = NAN;
  if (s.typ == Json::OBIEKT) {
    double v = liczbaLubBrak(s["co2"], OTO_CO2_MIN, OTO_CO2_MAX);
    o.co2 = std::isnan(v) ? -1 : (int16_t)std::lround(v);
    o.tempSyp = (float)liczbaLubBrak(s["temp"], -40, 60);
    o.wilgSyp = (float)liczbaLubBrak(s["wilg"], 0, 100);
    bool tsOk = tsBloku(s, teraz, o.tsSypialni);
    if (!tsOk) o.zleTs |= 4;
    o.sypialnia = tsOk && o.co2 >= 0;
  }
  o.co2Milczy = false;
  o.co2MilczyOd = 0;
  const Json& ost = j["ostrzezenia"];
  if (ost.typ == Json::TABLICA)
    for (size_t i = 0; i < ost.el.size(); i++) {
      const Json& x = ost[i];
      if (x.typ != Json::OBIEKT || x["kod"].typ != Json::TEKST || x["kod"].s != "co2_milczy") continue;  // nieznane
      o.co2Milczy = true;
      uint32_t e;
      o.co2MilczyOd = x["od"].typ == Json::TEKST && epochZIso(x["od"].s, e) ? e : 0;
    }
  o.stan = (int8_t)StanOto::OK;
  o.pobrano = teraz;
  return StanOto::OK;
}

void nieudanaOtoczenia(Otoczenie& o, int kod, uint32_t teraz) {
  o.proba = teraz;
  o.kod = (int16_t)(kod < -32000 ? -32000 : kod > 32000 ? 32000 : kod);
  if (kod == 401 || kod == 403) o.stan = (int8_t)StanOto::ODMOWA;
  else if (kod >= 300 && kod <= 399) o.stan = (int8_t)StanOto::PRZEKIEROWANIE;  // R-NF-27: bez podążania
  else if (kod <= 0 || kod >= 500) o.stan = (int8_t)StanOto::BRAK_ODPOWIEDZI;
  else o.stan = (int8_t)StanOto::HTTP;
}

bool brakOdpowiedziOtoczenia(int kod) { return kod <= 0 || kod >= 500; }

bool ponowicOtoczenie(int kod, int proba, long zostaloMs, int maxProb) {
  return brakOdpowiedziOtoczenia(kod) && proba < maxProb && proba < OTO_PROB &&
         zostaloMs >= (long)OTO_ODSTEP_PROB_MS + (long)OTO_LIMIT_ODPOWIEDZI_MS;
}

int co2DoBufora(Otoczenie& o, uint32_t teraz) {
  int c = co2Teraz(o, teraz);
  if (c < 0 || o.tsSypialni <= o.tsCo2WBuforze) return -1;
  o.tsCo2WBuforze = o.tsSypialni;
  return c;
}

// ---------- wartości do pokazania ----------
std::string pogodaTeraz(const Otoczenie& o, uint32_t teraz) {
  if (o.magia != OTO_MAGIA || !o.pogoda || std::isnan(o.temp) || !swiezy(o.tsPogody, teraz, OTO_SWIEZOSC_POGODY_S))
    return "";
  std::string s = o.miasto[0] ? std::string(o.miasto) + " " : std::string();
  s += calkowita(o.temp) + "°";
  if (o.opis[0]) s += std::string(" · ") + o.opis;
  return s;
}

int co2Teraz(const Otoczenie& o, uint32_t teraz) {
  if (o.magia != OTO_MAGIA || !o.sypialnia || o.co2 < 0 || !swiezy(o.tsSypialni, teraz, OTO_SWIEZOSC_SYPIALNI_S))
    return -1;
  return o.co2;
}

int pm25Teraz(const Otoczenie& o, uint32_t teraz) {
  if (o.magia != OTO_MAGIA || !o.powietrze || o.pm25 < 0 || !swiezy(o.tsPowietrza, teraz, OTO_SWIEZOSC_POGODY_S))
    return -1;
  return o.pm25;
}

bool co2MilczyTeraz(const Otoczenie& o, uint32_t teraz) {
  return o.magia == OTO_MAGIA && o.co2Milczy && swiezy(o.pobrano, teraz, OTO_SWIEZOSC_OSTRZEZEN_S);
}

std::string liniaTestuOtoczenia(const Otoczenie& o, uint32_t teraz, const std::string& kiedy) {
  std::string l = "Otoczenie: ";
  switch ((StanOto)o.stan) {
    case StanOto::WYLACZONE: return l + "wyłączone, brak OTO_URL w secrets.h";
    case StanOto::NIE_BYLO: return l + "jeszcze bez odczytu";
    case StanOto::BRAK_ODPOWIEDZI: l += "brak odpowiedzi (kod " + std::to_string(o.kod) + ")"; break;
    case StanOto::ODMOWA: l += "HTTP " + std::to_string(o.kod) + " (zły OTO_TOKEN)"; break;
    case StanOto::HTTP: l += "HTTP " + std::to_string(o.kod); break;
    case StanOto::PRZEKIEROWANIE: l += "HTTP " + std::to_string(o.kod) + " przekierowanie, bez podążania"; break;
    case StanOto::POZA_SIECIA: return l + "adres poza siecią domową, bez zapytań";
    case StanOto::ZLA_WERSJA:  // dane skasowane (jak brak źródła), więc bez „dane z”
      return l + "wersja " + (o.wersja < 0 ? std::string("?") : std::to_string(o.wersja)) + ", panel zna 1";
    case StanOto::NIECZYTELNE: l += "odpowiedź nieczytelna"; break;
    case StanOto::ZA_DUZE: l += "odpowiedź za duża"; break;
    case StanOto::OK: {
      l += "OK " + kiedy;
      std::string pg = pogodaTeraz(o, teraz);
      l += pg.empty() ? " · pogoda –" : " · " + calkowita(o.temp) + "°";
      int pm = pm25Teraz(o, teraz), c = co2Teraz(o, teraz);
      l += " · PM2.5 " + (pm < 0 ? std::string("–") : std::to_string(pm));
      l += " · CO2 " + (c < 0 ? std::string("–") : std::to_string(c));
      if (co2MilczyTeraz(o, teraz)) l += " · CO2 milczy";
      return l;
    }
  }
  // nieudana próba: na ekranie zostają dane z ostatniej udanej odpowiedzi (w granicach świeżości)
  l += kiedy.empty() ? std::string(" · brak wcześniejszych danych") : " · zostają dane z " + kiedy;
  return l;
}

std::vector<std::string> wartosciTestuOtoczenia(const Otoczenie& o, const std::string& kiedy) {
  switch ((StanOto)o.stan) {
    case StanOto::WYLACZONE: return {"wyłączone"};
    case StanOto::NIE_BYLO: return {"jeszcze bez odczytu"};
    case StanOto::OK: return {"OK " + kiedy};
    case StanOto::BRAK_ODPOWIEDZI: return {"brak odpowiedzi"};
    case StanOto::ODMOWA:
    case StanOto::HTTP: return {"HTTP " + std::to_string(o.kod)};
    case StanOto::PRZEKIEROWANIE: return {"przekierowanie"};
    case StanOto::POZA_SIECIA: return {"adres poza siecią domową", "poza siecią domową"};
    case StanOto::ZLA_WERSJA:
      return {o.wersja < 0 ? std::string("brak wersji kontraktu") : "wersja kontraktu " + std::to_string(o.wersja)};
    case StanOto::NIECZYTELNE: return {"odpowiedź nieczytelna", "nieczytelna"};
    case StanOto::ZA_DUZE: return {"odpowiedź za duża"};
  }
  return {"–"};
}

bool pokazE08(bool aktywne, bool bezOstrzezenia, uint32_t od, const std::string& dzis, StanE08& s, bool& zmiana) {
  zmiana = false;
  if (bezOstrzezenia && !s.dzien.empty() && !s.koniec) {  // epizod się skończył
    s.koniec = true;
    zmiana = true;
  }
  if (!aktywne || dzis.empty()) return false;
  if (s.dzien.empty() || s.od != od || (s.koniec && od == 0)) {  // nowy epizod (inne `od` albo znów `od` null): linia
    s.od = od;
    s.dzien = dzis;
    s.koniec = false;
    zmiana = true;
    return true;
  }
  if (s.koniec) {  // to samo znane `od` wróciło: ten sam epizod, bez nowej linii
    s.koniec = false;
    zmiana = true;
  }
  return s.dzien == dzis;
}

// R-NF-27: adres w sieci lokalnej (kontrakt w otoczenie.h). Sama analiza tekstu: schemat, host (bez `@`), port.
static bool oktet(const std::string& t, int& v) {
  if (t.empty() || t.size() > 3) return false;
  if (t.size() > 1 && t[0] == '0') return false;  // U1: zero wiodące = zapis ósemkowy w inet_aton (np. 010 = 8)
  v = 0;
  for (char c : t) {
    if (c < '0' || c > '9') return false;
    v = v * 10 + (c - '0');
  }
  return v <= 255;
}

bool adresWSieciLokalnej(const std::string& url) {
  const std::string schemat = "http://";
  if (url.compare(0, schemat.size(), schemat) != 0 || url.size() > 512) return false;
  // U1: znaki, przy których HTTPClient i ta funkcja mogą inaczej wyznaczyć host (dane logowania przed `@`, `\`
  // w niektórych parserach jak `/`), w całym adresie
  for (char z : url) {
    unsigned char c = (unsigned char)z;
    if (c <= 0x20 || c >= 0x7F || c == '@' || c == '\\') return false;
  }
  // host jak HTTPClient: do pierwszego `/`; `?` albo `#` przed nim = adres niejednoznaczny, odrzucony
  size_t koniec = url.find('/', schemat.size());
  std::string wladza = url.substr(schemat.size(), koniec == std::string::npos ? std::string::npos : koniec - schemat.size());
  if (wladza.empty() || wladza.find_first_of("?#[]") != std::string::npos) return false;
  if (std::count(wladza.begin(), wladza.end(), ':') > 1) return false;  // IPv6 albo dwa porty
  std::string host = wladza;
  size_t dw = wladza.find(':');
  if (dw != std::string::npos) {
    host = wladza.substr(0, dw);
    std::string port = wladza.substr(dw + 1);
    if (port.empty() || port.size() > 5) return false;
    long p = 0;
    for (char c : port) {
      if (c < '0' || c > '9') return false;
      p = p * 10 + (c - '0');
    }
    if (p < 1 || p > 65535) return false;
  }
  if (host.empty()) return false;
  // IPv4 w zapisie dziesiętnym z kropkami: tylko prywatne (RFC 1918) i link-local
  int o[4], n = 0;
  size_t a = 0;
  bool ipv4 = true;
  for (;;) {
    size_t b = host.find('.', a);
    std::string cz = host.substr(a, b == std::string::npos ? std::string::npos : b - a);
    if (n >= 4 || !oktet(cz, o[n])) {
      ipv4 = false;
      break;
    }
    n++;
    if (b == std::string::npos) break;
    a = b + 1;
  }
  if (ipv4 && n == 4)
    return o[0] == 10 || (o[0] == 172 && o[1] >= 16 && o[1] <= 31) || (o[0] == 192 && o[1] == 168) ||
           (o[0] == 169 && o[1] == 254);
  // U1: host z samych cyfr, kropek i liter zapisu szesnastkowego, który nie jest czterema liczbami dziesiętnymi
  // (np. `3232235777`, `0xC0A80101`, `010.0.0.5`, `192.168.1`), to dla resolvera adres IP, nie nazwa: odrzucony
  if (host.find_first_not_of("0123456789.xXabcdefABCDEF") == std::string::npos) return false;
  // nazwa mDNS: etykiety z liter, cyfr i `-`, ostatnia `local`
  const std::string sufiks = ".local";
  if (host.size() <= sufiks.size() || host.size() > 253) return false;
  std::string m;
  for (char c : host) m += (char)(c >= 'A' && c <= 'Z' ? c - 'A' + 'a' : c);
  if (m.compare(m.size() - sufiks.size(), sufiks.size(), sufiks) != 0) return false;
  size_t dl = 0;
  for (size_t i = 0; i < m.size(); i++) {
    char c = m[i];
    if (c == '.') {
      if (dl == 0) return false;  // pusta etykieta
      dl = 0;
      continue;
    }
    if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-') || ++dl > 63) return false;
  }
  return dl > 0;
}
