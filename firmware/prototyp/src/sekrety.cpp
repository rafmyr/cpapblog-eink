// SPDX-License-Identifier: GPL-3.0-or-later
#include "sekrety.h"

#include "wspolne.h"

namespace sekrety {
namespace {
// Klucze NVS (najwyżej 15 znaków). `zrodlo`: "k" z kompilacji, "f" z formularza, "z" zapis w toku (przerwany zapis
// nie daje mieszanki starych i nowych wartości: przy starcie jak brak), brak klucza: NVS pusty.
const char* const K_SSID = "ssid";
const char* const K_WIFI = "wifi";
const char* const K_AB = "ab";
const char* const K_KONTO = "ab_k";
const char* const K_HASLO = "ab_h";
const char* const K_ZRODLO = "zrodlo";
const char* const K_SKROT = "skrot";  // skrót wartości z secrets.h przy ostatniej kopii (źródło „k”)
// kopia ostatnio zatwierdzonych danych (te same pola z przedrostkiem `p_`) i jej źródło („k” albo „f”).
// Restart w trakcie zapisu (źródło „z”) wraca do kopii, a nie do secrets.h.
const char* const P_SSID = "p_ssid";
const char* const P_WIFI = "p_wifi";
const char* const P_AB = "p_ab";
const char* const P_KONTO = "p_ab_k";
const char* const P_HASLO = "p_ab_h";
const char* const P_ZRODLO = "p_zrodlo";

uint32_t skrot(const Dane& d) {
  std::string t = d.ssid + '\n' + d.hasloWifi + '\n' + d.abAdres + '\n' + d.abKonto + '\n' + d.abHaslo;
  return crc32((const uint8_t*)t.data(), t.size()) ^ 0x5EC2E7u;
}
bool rowne(const Dane& a, const Dane& b) {
  return a.ssid == b.ssid && a.hasloWifi == b.hasloWifi && a.abAdres == b.abAdres && a.abKonto == b.abKonto &&
         a.abHaslo == b.abHaslo;
}
const char* const GLOWNE[5] = {K_SSID, K_WIFI, K_AB, K_KONTO, K_HASLO};
const char* const KOPIA[5] = {P_SSID, P_WIFI, P_AB, P_KONTO, P_HASLO};
Dane czytaj(Nvs& n, const char* const kl[5]) {
  Dane d;
  d.ssid = n.tekst(PRZESTRZEN, kl[0], "");
  d.hasloWifi = n.tekst(PRZESTRZEN, kl[1], "");
  d.abAdres = n.tekst(PRZESTRZEN, kl[2], "");
  d.abKonto = n.tekst(PRZESTRZEN, kl[3], "");
  d.abHaslo = n.tekst(PRZESTRZEN, kl[4], "");
  return d;
}
// zapis z wynikiem i odczytem kontrolnym wszystkich pięciu kluczy (flash bywa zawodny po cichu).
bool zapisz(Nvs& n, const char* const kl[5], const Dane& d) {
  bool ok = n.ustawTekst(PRZESTRZEN, kl[0], d.ssid);
  ok = n.ustawTekst(PRZESTRZEN, kl[1], d.hasloWifi) && ok;
  ok = n.ustawTekst(PRZESTRZEN, kl[2], d.abAdres) && ok;
  ok = n.ustawTekst(PRZESTRZEN, kl[3], d.abKonto) && ok;
  ok = n.ustawTekst(PRZESTRZEN, kl[4], d.abHaslo) && ok;
  return ok && rowne(czytaj(n, kl), d);
}
Dane czytajWartosci(Nvs& n) { return czytaj(n, GLOWNE); }
// Kopia zatwierdzonych danych; zapis tylko przy różnicy.
bool zapiszKopie(Nvs& n, const Dane& d, const char* zr) {
  if (rowne(czytaj(n, KOPIA), d) && n.tekst(PRZESTRZEN, P_ZRODLO, "") == zr) return true;
  n.ustawTekst(PRZESTRZEN, P_ZRODLO, "");
  if (!zapisz(n, KOPIA, d)) return false;
  return n.ustawTekst(PRZESTRZEN, P_ZRODLO, zr) && n.tekst(PRZESTRZEN, P_ZRODLO, "") == zr;
}
// Przerwany albo nieudany zapis: dane główne z kopii, źródło z kopii. false = kopii nie ma albo zapis się nie udał.
bool przywrocKopie(Nvs& n) {
  std::string zr = n.tekst(PRZESTRZEN, P_ZRODLO, "");
  if (zr != "k" && zr != "f") return false;
  Dane d = czytaj(n, KOPIA);
  if (!skonfigurowany(d) || !zapisz(n, GLOWNE, d)) return false;
  return n.ustawTekst(PRZESTRZEN, K_ZRODLO, zr);
}
}  // namespace

bool skonfigurowany(const Dane& d) { return !d.ssid.empty() && !d.abAdres.empty(); }

const char* nazwaZrodla(Zrodlo z) {
  return z == Zrodlo::KOMPILACJA ? "secrets.h" : z == Zrodlo::FORMULARZ ? "formularz" : "brak (pierwsza konfiguracja)";
}

Wynik wczytaj(Nvs& n, const Dane& zk) {
  Wynik w;
  std::string zr = n.tekst(PRZESTRZEN, K_ZRODLO, "");
  if (zr == "z" && przywrocKopie(n)) zr = n.tekst(PRZESTRZEN, K_ZRODLO, "");  // restart w trakcie zapisu
  if (zr == "f") {
    w.d = czytajWartosci(n);
    if (skonfigurowany(w.d)) {
      w.zrodlo = Zrodlo::FORMULARZ;
      return w;
    }
  }
  bool kompilacja = skonfigurowany(zk);
  if (zr == "k") {
    Dane d = czytajWartosci(n);
    // te same wartości z kompilacji co przy ostatniej kopii (albo secrets.h już bez nich): NVS
    if (skonfigurowany(d) && (!kompilacja || n.liczbaU(PRZESTRZEN, K_SKROT, 0) == skrot(zk))) {
      w.d = d;
      w.zrodlo = Zrodlo::KOMPILACJA;
      return w;
    }
  }
  if (!kompilacja) return w;  // pierwsza konfiguracja: nic nie zapisane
  n.ustawTekst(PRZESTRZEN, K_ZRODLO, "z");
  if (!zapisz(n, GLOWNE, zk)) {
    przywrocKopie(n);  // nieudany zapis: poprzednie dane z kopii, gdy była; panel i tak pracuje z secrets.h
  } else {
    n.ustawLiczbeU(PRZESTRZEN, K_SKROT, skrot(zk));
    n.ustawTekst(PRZESTRZEN, K_ZRODLO, "k");
    zapiszKopie(n, zk, "k");
  }
  w.d = zk;
  w.zrodlo = Zrodlo::KOMPILACJA;
  w.zapisanoPoczatkowe = true;
  return w;
}

bool zapiszZFormularza(Nvs& n, const Dane& d) {
  if (!skonfigurowany(d)) return false;
  // najpierw kopia = nowe dane (klucze główne i źródło jeszcze stare: restart tu zostawia stare),
  // potem „z”, klucze główne, „f”. Restart po „z” przywraca z kopii, która jest już kompletna i sprawdzona (nowe).
  std::string zr = n.tekst(PRZESTRZEN, K_ZRODLO, "");
  Dane obecne = czytajWartosci(n);
  bool byly = (zr == "k" || zr == "f") && skonfigurowany(obecne);
  if (!zapiszKopie(n, d, "f")) return false;  // klucze główne bez zmian
  if (!n.ustawTekst(PRZESTRZEN, K_ZRODLO, "z")) {
    if (byly) n.ustawTekst(PRZESTRZEN, K_ZRODLO, zr);
    return false;
  }
  if (!zapisz(n, GLOWNE, d)) {  // nieudany zapis albo odczyt kontrolny: poprzednie dane wracają
    if (byly && zapisz(n, GLOWNE, obecne)) {
      n.ustawTekst(PRZESTRZEN, K_ZRODLO, zr);
      zapiszKopie(n, obecne, zr.c_str());
    }
    return false;
  }
  return n.ustawTekst(PRZESTRZEN, K_ZRODLO, "f") && n.tekst(PRZESTRZEN, K_ZRODLO, "") == "f";
}

std::string opis(const Wynik& w) {
  if (w.pierwszaKonfiguracja()) return "brak sieci albo adresu AirBridge: pierwsza konfiguracja (R-KONF-07)";
  return "sieć " + w.d.ssid + ", AirBridge " + w.d.abAdres + ", konto " + (w.d.abKonto.empty() ? "-" : w.d.abKonto) +
         ", hasło Wi-Fi " + (w.d.hasloWifi.empty() ? "puste" : "ustawione") + ", hasło AirBridge " +
         (w.d.abHaslo.empty() ? "puste" : "ustawione") + ", źródło " + nazwaZrodla(w.zrodlo) +
         (w.zapisanoPoczatkowe ? " (skopiowane do NVS)" : "");
}

std::string kluczWydan(Nvs& n) { return n.tekst(PRZESTRZEN, "gh_tok", ""); }

bool zapiszKluczWydan(Nvs& n, const std::string& t) {
  if (n.tekst(PRZESTRZEN, "gh_tok", "") == t) return true;
  bool ok = n.ustawTekst(PRZESTRZEN, "gh_tok", t);
  return ok && n.tekst(PRZESTRZEN, "gh_tok", "") == t;
}

namespace {
bool zeSterujacym(const std::string& s) {
  for (unsigned char c : s)
    if (c < 0x20 || c == 0x7F) return true;
  return false;
}
}  // namespace

std::string wczytajHasloOkna(Nvs& n, const std::string& zk, bool zSekretami) {
  std::string zr = n.tekst(PRZESTRZEN, "ota_zr", "");
  std::string h = n.tekst(PRZESTRZEN, "ota_h", "");
  if ((zr == "f" && !h.empty()) || !zSekretami) return h;  // formularz wygrywa; obraz wydania: NVS bez zmian
  if (h != zk) n.ustawTekst(PRZESTRZEN, "ota_h", zk);
  std::string nz = zk.empty() ? "" : "k";
  if (zr != nz) n.ustawTekst(PRZESTRZEN, "ota_zr", nz);
  return zk;
}

bool zapiszHasloOkna(Nvs& n, const std::string& h) {
  if (h.size() < HASLO_OKNA_MIN_ZNAKOW || h.size() > 64 || zeSterujacym(h)) return false;
  bool ok = n.ustawTekst(PRZESTRZEN, "ota_h", h);
  ok = n.ustawTekst(PRZESTRZEN, "ota_zr", "f") && ok;
  return ok && n.tekst(PRZESTRZEN, "ota_h", "") == h && n.tekst(PRZESTRZEN, "ota_zr", "") == "f";
}

bool hasloOknaUstawione(Nvs& n) { return !n.tekst(PRZESTRZEN, "ota_h", "").empty(); }

Dodatki wczytajDodatki(Nvs& n, const Dodatki& zk, bool zSekretami) {
  Dodatki d;
  auto pole = [&n, zSekretami](const char* k, const std::string& v) {
    if (!zSekretami) return n.tekst(PRZESTRZEN, k, "");
    if (n.tekst(PRZESTRZEN, k, "") != v) n.ustawTekst(PRZESTRZEN, k, v);
    return v;
  };
  d.otoUrl = pole("oto_url", zk.otoUrl);
  d.otoToken = pole("oto_tok", zk.otoToken);
  bool para = !zk.pogodaLat.empty() && !zk.pogodaLon.empty();  // bez pary: obie puste (bez mieszania miast)
  d.pogodaLat = pole("pog_lat", para ? zk.pogodaLat : "");
  d.pogodaLon = pole("pog_lon", para ? zk.pogodaLon : "");
  return d;
}

}  // namespace sekrety
