// SPDX-License-Identifier: GPL-3.0-or-later
// Limity czasu operacji sieciowych i znacznik „w sieci” (src/siec_limity.h). Testy: test/testy_awarie_sieci.cpp.
#include "siec_limity.h"
#include "start.h"

namespace siec_limity {

uint32_t przestojCialaMs(const ZadanieHttp& z) { return z.limitMs ? z.limitMs : PRZESTOJ_DOMYSLNY_MS; }

uint32_t limitCialaMs(const ZadanieHttp& z) {
  return z.budzetCialaMs && z.budzetCialaMs < CIALO_NAJDLUZEJ_MS ? z.budzetCialaMs : CIALO_NAJDLUZEJ_MS;
}

StanCiala stanCiala(const ZadanieHttp& z, uint32_t startMs, uint32_t ostatniMs, uint32_t terazMs) {
  if (terazMs - startMs >= limitCialaMs(z)) return StanCiala::ZA_DLUGO;
  if (terazMs - ostatniMs >= przestojCialaMs(z)) return StanCiala::PRZESTOJ;
  return StanCiala::CZYTAJ;
}

bool hostJestNazwa(const std::string& url) {
  size_t a = url.find("://");
  a = a == std::string::npos ? 0 : a + 3;
  size_t b = url.find_first_of(":/?", a);
  std::string h = url.substr(a, b == std::string::npos ? std::string::npos : b - a);
  if (h.empty()) return false;
  int kropek = 0, cyfr = 0;
  for (char c : h) {
    if (c == '.') {
      if (!cyfr) return true;
      kropek++;
      cyfr = 0;
    } else if (c >= '0' && c <= '9') {
      if (++cyfr > 3) return true;
    } else {
      return true;
    }
  }
  return kropek != 3 || !cyfr;
}

uint32_t najdluzszaPrzerwaGetMs(const ZadanieHttp& z) {
  uint32_t l = z.limitMs ? z.limitMs : PRZESTOJ_DOMYSLNY_MS;
  return (hostJestNazwa(z.url) ? DNS_NAJDLUZEJ_MS : 0) + l + WYSYLKA_NAJDLUZEJ_MS + l + przestojCialaMs(z);
}

uint32_t najdluzszaPrzerwaHttpsMs(uint32_t limitPolaczeniaMs) {
  return DNS_NAJDLUZEJ_MS + 3 * limitPolaczeniaMs + WYSYLKA_NAJDLUZEJ_MS;
}

uint32_t dozwolonaPrzerwaMs(uint32_t limitStraznikaS) {
  uint32_t l = limitStraznikaS * 1000u;
  return l > ZAPAS_STRAZNIKA_MS ? l - ZAPAS_STRAZNIKA_MS : 0;
}

bool DekoderChunked::dodaj(const uint8_t* b, size_t n, std::string& wyj) {
  for (size_t i = 0; i < n; i++) {
    uint8_t c = b[i];
    switch (stan_) {
      case Stan::ROZMIAR: {
        int v = c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1;
        if (v >= 0) {
          if (++cyfr_ > 8) {
            stan_ = Stan::BLAD;
            return false;
          }
          rozmiar_ = rozmiar_ * 16u + (uint32_t)v;
        } else if (cyfr_ && (c == ';' || c == ' ' || c == '\t')) {
          stan_ = Stan::ROZSZERZENIE;
        } else if (cyfr_ && c == '\r') {
          stan_ = Stan::LF_ROZMIARU;
        } else {
          stan_ = Stan::BLAD;
          return false;
        }
        break;
      }
      case Stan::ROZSZERZENIE:
        if (c == '\r') stan_ = Stan::LF_ROZMIARU;
        break;
      case Stan::LF_ROZMIARU:
        if (c != '\n') {
          stan_ = Stan::BLAD;
          return false;
        }
        if (rozmiar_ == 0) {
          stan_ = Stan::KONIEC;
          return true;
        }
        zostalo_ = rozmiar_;
        stan_ = Stan::DANE;
        break;
      case Stan::DANE: {
        size_t k = n - i < zostalo_ ? n - i : zostalo_;
        wyj.append((const char*)b + i, k);
        zostalo_ -= (uint32_t)k;
        i += k - 1;
        if (!zostalo_) stan_ = Stan::CR_DANYCH;
        break;
      }
      case Stan::CR_DANYCH:
        if (c != '\r') {
          stan_ = Stan::BLAD;
          return false;
        }
        stan_ = Stan::LF_DANYCH;
        break;
      case Stan::LF_DANYCH:
        if (c != '\n') {
          stan_ = Stan::BLAD;
          return false;
        }
        rozmiar_ = 0;
        cyfr_ = 0;
        stan_ = Stan::ROZMIAR;
        break;
      case Stan::KONIEC: return true;
      case Stan::BLAD: return false;
    }
  }
  return stan_ != Stan::BLAD;
}

int czytajCialo(StrumienCiala& s, const ZadanieHttp& z, uint32_t t0, bool chunked, OdpowiedzHttp& o) {
  const long dl = chunked ? -1 : (long)o.contentLength;
  DekoderChunked dek;
  uint8_t buf[512];
  uint32_t ostatni = s.ms();
  size_t surowych = 0;
  for (;;) {
    if (chunked ? dek.koniec() : (dl >= 0 && (long)surowych >= dl)) return (int)o.cialo.size();
    uint32_t t = s.ms();
    StanCiala st = stanCiala(z, t0, ostatni, t);
    if (st != StanCiala::CZYTAJ) {
      o.zaDlugo = st == StanCiala::ZA_DLUGO;
      return -11;
    }
    if (z.przerwij && z.przerwij()) {
      o.przerwane = true;
      return -10;
    }
    size_t chce = sizeof buf;
    if (!chunked && dl >= 0 && (size_t)(dl - (long)surowych) < chce) chce = (size_t)(dl - (long)surowych);
    int n = s.dostepne() > 0 ? s.czytaj(buf, chce) : 0;
    if (n <= 0) {
      if (!s.polaczony()) return chunked || dl >= 0 ? -11 : (int)o.cialo.size();  // bez długości: koniec = zamknięcie
      s.czekaj(OBIEG_CIALA_MS);
      continue;
    }
    ostatni = t;
    s.odnow();
    surowych += (size_t)n;
    size_t przed = o.cialo.size();
    if (chunked) {
      if (!dek.dodaj(buf, (size_t)n, o.cialo)) return -7;
    } else {
      o.cialo.append((const char*)buf, (size_t)n);
    }
    if (z.maksBajtow && o.cialo.size() > z.maksBajtow) {
      o.cialo.resize(przed);
      o.zaDuzo = true;
      return -10;
    }
  }
}

void SiecZeZnacznikiem::znacznik(bool w) {
  if (w) wejscieWSiec(u_.licznik(), identyfikatorWersji(u_.wersja()));
  else wyjscieZSieci(u_.licznik(), identyfikatorWersji(u_.wersja()));
}

bool SiecZeZnacznikiem::polacz(uint32_t limitMs, const std::function<bool()>& przerwij, bool& przerwane) {
  przerwane = false;
  if (bezSieci) return false;
  znacznik(true);
  bool w = s_.polacz(limitMs, przerwij, przerwane);
  znacznik(false);
  return w;
}

std::string SiecZeZnacznikiem::opisBledu() {
  if (bezSieci) return "bez sieci po restarcie strażnika w trakcie pracy w sieci (R-AW-39)";
  znacznik(true);
  std::string o = s_.opisBledu();
  znacznik(false);
  return o;
}

bool SiecZeZnacznikiem::czasZSieci(uint32_t limitMs) {
  if (bezSieci) return false;
  znacznik(true);
  bool w = s_.czasZSieci(limitMs);
  znacznik(false);
  return w;
}

OdpowiedzHttp SiecZeZnacznikiem::get(const ZadanieHttp& z) {
  if (bezSieci) {
    OdpowiedzHttp o;
    o.kod = -1;  // jak HTTPC_ERROR_CONNECTION_REFUSED
    return o;
  }
  znacznik(true);
  OdpowiedzHttp o = s_.get(z);
  znacznik(false);
  return o;
}

void WydaniaZeZnacznikiem::znacznik(bool w) {
  if (w) wejscieWSiec(u_.licznik(), identyfikatorWersji(u_.wersja()));
  else wyjscieZSieci(u_.licznik(), identyfikatorWersji(u_.wersja()));
}

OdpowiedzWydania WydaniaZeZnacznikiem::pobierzTekst(const ZadanieWydania& z) {
  znacznik(true);
  OdpowiedzWydania o = w_->pobierzTekst(z);
  znacznik(false);
  return o;
}

WynikObrazu WydaniaZeZnacznikiem::pobierzObraz(const ZadanieWydania& z, uint64_t rozmiar,
                                               const std::function<void()>& postep) {
  znacznik(true);
  WynikObrazu o = w_->pobierzObraz(z, rozmiar, postep);
  znacznik(false);
  return o;
}

}  // namespace siec_limity
