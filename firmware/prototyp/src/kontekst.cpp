// SPDX-License-Identifier: GPL-3.0-or-later
// Ekrany kontekstowe CO2: opis w src/kontekst.h. Bez wyjątków i RTTI.
#include "kontekst.h"
#include <algorithm>
#include <cstdlib>
#include "wspolne.h"

Co2Nocy statystykaCo2(std::vector<ProbkaCo2> p) {
  Co2Nocy n;
  p.erase(std::remove_if(p.begin(), p.end(), [](const ProbkaCo2& x) { return x.ppm < 0 || x.ts == 0; }), p.end());
  std::sort(p.begin(), p.end(), [](const ProbkaCo2& a, const ProbkaCo2& b) { return a.ts < b.ts; });
  for (size_t i = 0; i < p.size(); i++) {
    n.odczytow++;
    n.wartosci.push_back(p[i].ppm);
    n.maks = std::max(n.maks, p[i].ppm);
    uint32_t dt = i + 1 < p.size() ? p[i + 1].ts - p[i].ts : CO2_PROBKA_MAX_S;
    if (dt > CO2_PROBKA_MAX_S) dt = CO2_PROBKA_MAX_S;
    if (p[i].ppm > CO2_PROG) n.minutPonad += (int)(dt / 60);
  }
  return n;
}

Co2Nocy co2ZNocy(const NocSypialni* n) {
  Co2Nocy c;
  if (!n || n->co2Ponad < 0) return c;
  c.minutPonad = n->co2Ponad;
  c.maks = n->co2Maks;
  size_t a = 0;
  while (a < n->co2Seria.size()) {
    size_t b = n->co2Seria.find(',', a);
    std::string x = n->co2Seria.substr(a, b == std::string::npos ? std::string::npos : b - a);
    int v = 0;
    if (indeksZTekstu(x.c_str(), 10000, v)) c.wartosci.push_back(v);
    if (b == std::string::npos) break;
    a = b + 1;
  }
  c.odczytow = (int)c.wartosci.size();
  return c;
}

bool sezonGrzewczy(const std::string& d) {
  if (!dataOk(d)) return false;
  int m = liczbaCalk(d.substr(5, 2).c_str());
  return m >= 10 || m <= 4;
}

namespace {
// Pokazania (nie wpisy logu trybu próbnego: te nie trafiają do dziennika ekranów) typu `typ` przed dniem `dzis`.
std::vector<const WpisHumoru*> pokazania(const std::vector<WpisHumoru>* d, const char* typ, const std::string& dzis) {
  std::vector<const WpisHumoru*> w;
  if (!d) return w;
  for (auto& x : *d)
    if (x.typ == typ && x.data < dzis) w.push_back(&x);
  return w;
}
std::string poczatekSezonu(const std::string& d) {
  int r = liczbaCalk(d.substr(0, 4).c_str()), m = liczbaCalk(d.substr(5, 2).c_str());
  char b[16];
  snprintf(b, sizeof b, "%04d-10-01", m >= 10 ? r : r - 1);
  return b;
}
}  // namespace

WynikWieczoru ocenWieczor(const WejscieWieczoru& w) {
  WynikWieczoru r;
  bool zNocy = w.noc && w.noc->minutPonad >= SK01_MIN_PONAD;
  bool teraz = w.co2Teraz >= CO2_PROG;
  if (!zNocy && !teraz) { r.dlaczego = "CO2 poniżej progu albo brak odczytu"; return r; }
  if (w.prosbaDzis) { r.dlaczego = "dziś była już prośba (7.4, R-PIE-10)"; return r; }
  if (w.wieczorowZKrokiem7 >= 4) { r.dlaczego = "4 wieczory z krokiem w 7 dniach (R-PIE-12)"; return r; }
  r.zOdczytu = !zNocy;
  r.wartosc = zNocy ? w.noc->minutPonad : w.co2Teraz;
  // smog za oknem: S-K-02, gdy znamy PM2.5 w sypialni (tekst powodu go podaje), inaczej zastępczy próg blokuje S-K-01
  bool pmZnany = !std::isnan(w.pm25ZaOknem);
  if (pmZnany && !std::isnan(w.pm25Sypialni)) {
    if (w.pm25ZaOknem > SK02_PM25_ZA_OKNEM && w.pm25ZaOknem > w.pm25Sypialni) {
      r.ekran = EkranWieczoru::SK02;
    } else if (w.pm25ZaOknem > w.pm25Sypialni) {
      r.dlaczego = "PM2.5 za oknem wyższe niż w sypialni";
      return r;
    }
  } else if (pmZnany && w.pm25ZaOknem > PM25_ZA_OKNEM_ZASTEPCZY) {
    r.dlaczego = "smog za oknem (próg zastępczy, bez PM2.5 sypialni)";
    return r;
  }
  if (r.ekran == EkranWieczoru::BRAK) r.ekran = EkranWieczoru::SK01;
  // limity: sezon grzewczy (R-KON-29) albo „ten sam ekran wieczorny” raz na 3 dni z wyjątkiem ≥ 25% (7.4, R-KON-04)
  const char* typ = r.ekran == EkranWieczoru::SK01 ? "SK01" : "SK02";
  std::vector<const WpisHumoru*> p = pokazania(w.dziennik, typ, w.dzis);
  const WpisHumoru* ost = p.empty() ? nullptr : p.back();
  for (auto* x : p)
    if (x->data > (ost ? ost->data : "")) ost = x;
  if (r.ekran == EkranWieczoru::SK01 && sezonGrzewczy(w.dzis)) {
    std::string od = poczatekSezonu(w.dzis);
    int wSezonie = 0;
    for (auto* x : p)
      if (x->data >= od) wSezonie++;
    if (ost && dniMiedzy(ost->data, w.dzis) < 7) { r.ekran = EkranWieczoru::BRAK; r.dlaczego = "sezon grzewczy: raz na 7 dni (R-KON-29)"; return r; }
    // po 4 pokazaniach tylko przy osobistym związku z wyciekiem albo czasem z maską; tego związku nie liczy, więc
    // ekran milczy do końca sezonu (wypisane w raporcie)
    if (wSezonie >= 4) { r.ekran = EkranWieczoru::BRAK; r.dlaczego = "sezon grzewczy: 4 pokazania bez policzonego związku (R-KON-29)"; return r; }
  } else if (ost && dniMiedzy(ost->data, w.dzis) < 3) {
    int poprzednia = w.poprzedniaWartosc;
    // wyjątek tylko przy wyzwalaczu tego samego rodzaju (ppm z ppm, minuty nocy z minutami)
    if (poprzednia <= 0 || r.zOdczytu != w.poprzedniZOdczytu || r.wartosc < poprzednia * 1.25) {
      r.ekran = EkranWieczoru::BRAK;
      r.dlaczego = "ten sam ekran wieczorny: raz na 3 dni (7.4)";
      return r;
    }
  }
  if (w.trybProbny) r.doLogu = true;  // R-KON-05: wpis w logu, bez ekranu
  return r;
}

WynikRanka ocenRano(const WejscieRanka& w) {
  WynikRanka r;
  if (!w.noc || w.noc->odczytow == 0) { r.dlaczego = "brak serii CO2 nocy"; return r; }
  if (!(w.noc->minutPonad >= SK13_MIN_PONAD || w.noc->maks > SK13_MAKS)) { r.dlaczego = "warunek S-K-13 niespełniony"; return r; }
  if (w.pozaDomem) { r.dlaczego = "spanie poza domem (R-CZY-12)"; return r; }
  if (w.poprzedniWieczorSK01) { r.dlaczego = "wieczorem był S-K-01 (R-KON-20)"; return r; }
  std::vector<const WpisHumoru*> p = pokazania(w.dziennik, "SK13", w.dzis);
  for (auto* x : p)
    if (dniMiedzy(x->data, w.dzis) < 7) { r.dlaczego = "S-K-13 raz na 7 dni (R-KON-20)"; return r; }
  r.sk13 = true;
  r.doLogu = w.trybProbny;
  return r;
}

TrescKontekstu trescSK01(const WejscieWieczoru& w, const WynikWieczoru& r) {
  TrescKontekstu k;
  k.t.etykieta = "Przed snem";
  k.t.tytul = "Przewietrz sypialnię";
  // powód z wyzwalacza wyboru (r): przy ponownym rysowaniu tego samego dnia dyrygent podaje wynik zapamiętany
  // z pierwszego wyboru, nie ocenę z tej chwili; bez wyzwalacza bez powodu, nigdy `0 h`
  if (r.ekran != EkranWieczoru::BRAK && r.zOdczytu && r.wartosc >= CO2_PROG)
    k.t.powod = "Teraz CO2 " + std::to_string(r.wartosc) + " ppm. Powietrze jest zastałe.";
  else if (r.ekran != EkranWieczoru::BRAK && !r.zOdczytu && r.wartosc >= SK01_MIN_PONAD)
    k.t.powod = "Zeszłej nocy CO2 przez " + std::to_string(r.wartosc / 60) + "\xC2\xA0h ponad 1000\xC2\xA0ppm.";
  // tabela konfliktów 5.8.5 (pierwszy pasujący wiersz; smog to S-K-02)
  if (!std::isnan(w.tempSypialni) && w.tempSypialni <= 16) k.t.krok = "→ Uchyl okno, owiń rurę otuliną.";
  else if (w.pylekWysoki) k.t.krok = "→ Wietrz krótko, dużo pyłku.";
  else k.t.krok = "→ Uchyl okno na noc.";
  if (w.noc && w.noc->odczytow >= 2) {
    k.linia = w.noc->wartosci;
    k.podpisLewo = "CO2 zeszłej nocy, 23:00–06:00";
    k.podpisPrawo = "linia przerywana = 1000\xC2\xA0ppm";
  }
  // karta K05: fakty zamiast wizualizacji (5.7.2)
  int ppm = r.zOdczytu ? r.wartosc : w.co2Teraz;
  if (ppm >= 0) k.t.fakty.push_back({std::to_string(ppm), "ppm teraz"});
  if (!std::isnan(w.pm25ZaOknem)) k.t.fakty.push_back({calkowita(w.pm25ZaOknem), "PM2.5 za oknem"});
  return k;
}

TrescKontekstu trescSK02(const WejscieWieczoru& w) {
  TrescKontekstu k;
  k.t.etykieta = "Przed snem";
  k.t.tytul = "Smog za oknem";
  k.t.powod = "Pył za oknem " + calkowita(w.pm25ZaOknem) + ", w pokoju " + calkowita(w.pm25Sypialni) +
              ". Długie wietrzenie go wpuści.";
  k.t.krok = "→ Wietrz krótko, na noc zamknij.";
  k.pary.push_back({calkowita(w.pm25ZaOknem), "PM2.5 za oknem"});
  k.pary.push_back({calkowita(w.pm25Sypialni), "PM2.5 w sypialni"});
  if (w.co2Teraz >= 0) k.pary.push_back({std::to_string(w.co2Teraz), "CO2 w sypialni"});
  return k;
}

TrescKontekstu trescSK13(const Co2Nocy& n) {
  TrescKontekstu k;
  k.t.etykieta = "Ostatnia noc";
  k.t.tytul = "Zaduch w sypialni w nocy";
  k.t.powod = "CO2 w nocy doszło do " + std::to_string(n.maks) + "\xC2\xA0ppm. Wtedy sen bywa płytszy.";
  k.t.krok = "→ Dziś wieczorem spróbuj z uchylonym oknem.";
  if (n.odczytow >= 2) {
    k.linia = n.wartosci;
    k.podpisLewo = "CO2 w nocy, 23:00–06:00";
    k.podpisPrawo = "linia przerywana = 1000\xC2\xA0ppm";
  }
  return k;
}
