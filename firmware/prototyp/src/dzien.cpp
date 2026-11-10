// SPDX-License-Identifier: GPL-3.0-or-later
#include "dzien.h"
#include "ekrany_zdarzen.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "humor.h"
#include "kokpit.h"
#include "wspolne.h"

std::string kiedyOdczyt(const std::string& data, int minuta, const std::string& dzis) {
  if (!dataOk(data) || minuta < 0) return "";
  char b[16];
  snprintf(b, sizeof b, "%02d:%02d", minuta / 60 % 24, minuta % 60);
  return data == dzis ? std::string(b) : ddmm(data) + " " + b;
}

std::string opisNocyPytania(const Noc& n, const Norma& nm, bool innaNoc) {
  if (innaNoc) {
    Odchylenie o = odchylenieWNocy(n, nm);
    std::vector<std::string> cz;
    if (o.ahi) cz.push_back("AHI " + przecinek1(n.ahi) + " przy zwykłych " + przecinek1(nm.ahi.p10) + "–" + przecinek1(nm.ahi.p90));
    if (o.m01) cz.push_back("maska " + godzMin(n.m01) + " przy zwykłych " + godzMin(nm.m01.p10) + "–" + godzMin(nm.m01.p90));
    if (o.wp95 && cz.size() < 2)
      cz.push_back("wyciek " + calkowita(n.wp95) + " l/min przy zwykłych " + calkowita(nm.wp95.p10) + "–" + calkowita(nm.wp95.p90));
    std::string s;
    for (size_t i = 0; i < cz.size() && i < 2; i++) s += (i ? ", " : "") + cz[i];
    if (!s.empty()) return s;
  }
  return godzMin(n.m01) + " · AHI " + przecinek1(n.ahi);
}

std::string naglowekDniaLewo(const DaneDnia& d) {
  if (!dataOk(d.dzis)) return "bez zegara";
  return std::string(DNI_KROTKO[dzienTygodnia(d.dzis)]) + " " + ddmm(d.dzis);
}

std::string naglowekDniaPrawo(const DaneDnia& d) {
  const Noc* n = d.hist && dataOk(d.dzis) ? ostatniaNoc(*d.hist, d.dzis) : nullptr;
  if (!n) return "";
  if (n->data != d.dzis) return "dane z " + ddmm(n->data);  // R-EKR-39
  std::string p = d.godzinaDanych.empty() ? std::string("dane") : "dane " + d.godzinaDanych;  // R-EKR-04
  if (nocNiepewna(*n)) p += " · ocena ograniczona";  // R-EKR-46 (a)
  return p;
}

bool nocWNormie(const Noc& n, const Stany& st) {
  // 5.4.2: `W normie` tylko bez odchylenia w jednej nocy (4.5) i przy spełnionym warunku uspokojenia (5.4.13: także
  // bez punktów nieocenianych, Stany::ocenaNiepelna); noc nieważna (< 4 h) nie ma oceny odchylenia (4.5); noc
  // niepewna nie dostaje oceny (R-EKR-46)
  if (nocNiepewna(n)) return false;
  bool ocena = n.data == st.ostatnia && n.wazna;
  return ocena && n.m01 >= 0 && !st.odchylenie.jakiekolwiek() && st.uspokojenie;
}

std::string tytulNocy(const Noc& n, const Stany& st) {
  // nocWNormie wyklucza noc niepewną (R-EKR-46), nieważną i bez czasu z maską. Inaczej czas z maską bez oceny, żeby
  // werdykt nocy był czytelny z daleka (R-NF-23); bez czasu `Noc zapisana.`
  if (nocWNormie(n, st)) return "Noc w normie.";  // 5.4.2, karta K22 (dosłownie ze spec)
  return n.m01 >= 0 ? "Noc: " + godzH(n.m01) + " z maską." : std::string("Noc zapisana.");
}

// Fakty karty K22: czas z maską tylko wtedy, gdy nie stoi już w tytule (liczby nocy raz)
static std::vector<Fakt> faktyK22(const Noc& k, const std::string& tytul, const Fakt& trzeci) {
  std::vector<Fakt> f;
  if (tytul.rfind("Noc: ", 0) != 0) f.push_back({godzH(k.m01), "z maską"});
  // noc niepewna (4.9, R-EKR-46): AHI `–` z podpisem `AHI niepewne`, jak w faktyPrzerywnika() (ADR-0029 pkt 7)
  f.push_back(nocNiepewna(k) ? Fakt{"–", "AHI niepewne"} : Fakt{przecinek1(k.ahi), "AHI"});  // brak = „–” (R-DANE-07)
  f.push_back(trzeci);
  return f;
}

std::vector<Fakt> faktyPrzerywnika(const DaneDnia& d) {
  const Noc* k = d.hist && dataOk(d.dzis) ? ostatniaNoc(*d.hist, d.dzis) : nullptr;
  if (!k) return {};
  // noc niepewna (4.9, R-EKR-46): AHI `–` z podpisem `AHI niepewne`, jak na karcie K22 (ADR-0029 pkt 7)
  Fakt ahi = nocNiepewna(*k) ? Fakt{"–", "AHI niepewne"} : Fakt{przecinek1(k->ahi), "AHI"};
  return {{k->m01 >= 0 ? godzH(k->m01) : std::string("–"), "z maską"}, ahi};
}

void trescPrzerywnika(TrescEkranu& t, const std::string& tytul, const std::string& powod, const DaneDnia& d) {
  t.fakty = faktyPrzerywnika(d);
  t.etykietaKarty.clear();
  t.kartaBezEtykiety = true;
  t.tytul = tytul;
  t.powod = powod;
  t.powodKarty = powod;
  t.krok.clear();
}

namespace {

const Stany& stanyDnia(const DaneDnia& d, Stany& zapas) {
  if (d.stany) return *d.stany;
  static const std::vector<Noc> pusta;
  zapas = liczStany(d.hist ? *d.hist : pusta, d.dzis, {});
  return zapas;
}

}  // namespace

// Blok liczb ostatniej nocy na ekranie danych S2a i S2c (wariant B, 5.4.2): podpis, czas i AHI zamiast zdania
// `Noc: …`; noc niepewna: w miejscu AHI `nie oceniam: duży wyciek` (R-EKR-46); `W normie` jako początek zdania (tylko
// ekran danych: na karcie K22 `Noc w normie.` stoi w tytule). powodKarty: zdanie karty, która nie ma paska.
static void blokNocy(TrescEkranu& t, const DaneDnia& d, const Noc* k, const Stany& st, const std::string& powod,
                     const std::string& powodKarty) {
  t.podpisNocy = !k || k->data == d.dzis ? std::string("Ostatnia noc") : "Noc " + ddmm(k->data);
  t.czas = k ? godzH(k->m01) : "–";  // brak = „–” (R-DANE-07), nigdy 0 h 00
  t.ahi = k ? przecinek1(k->ahi) : "–";
  t.ahiNiepewne = k && nocNiepewna(*k);
  t.wNormie = k && nocWNormie(*k, st);
  // po przedrostku `Noc w normie. ` zdanie o różnicach tygodnia w krótszej formie (5.4.2, R-EKR-41; pełna
  // forma z przedrostkiem ma 785 px w kroju 34 px)
  t.powod = t.wNormie && powod == "Tydzień: różnice zaznaczone niżej." ? "Tydzień: różnice niżej." : powod;
  t.powodKarty = powodKarty;
}

// słowo porównania (porownajTydzien, slowoSypialni: `↔ jak zwykle`, `↑ wyżej niż zwykle`, `uczę się`, `–`) → kolumna
// paska: strzałka za wartością, słowo pod nią (przy `jak zwykle` puste: mówi legenda), ramka przy `wyżej/niżej`
static Kolumna kolumna(const std::string& nazwa, const std::string& wartosc, const std::string& slowo, bool spokojne) {
  Kolumna k;
  k.et = nazwa;
  k.w = wartosc;
  for (const char* s : {"↔", "↑", "↓"})
    if (slowo.rfind(s, 0) == 0) {
      k.s = s;
      k.p = slowo.substr(strlen(s) + 1);
    }
  if (k.s.empty()) k.p = slowo == "–" ? std::string() : slowo;  // `uczę się` bez strzałki
  if (k.p == "jak zwykle") k.p.clear();
  k.poza = !k.s.empty() && k.s != "↔" && !spokojne;
  return k;
}

static float poleCmedDnia(const Noc& n) { return n.cmed; }

static TrescEkranu s2a(const DaneDnia& d) {
  TrescEkranu t;
  t.uklad = Uklad::TYDZIEN;
  t.etykieta = "OSTATNIA NOC";
  t.etykietaKarty = "OSTATNIA NOC";  // karta K22: fakty ostatniej nocy
  t.tytulPaska = "Ostatnie 7 nocy · mediana";
  Stany zapas;
  const Stany& st = stanyDnia(d, zapas);
  const Norma& nm = d.stany ? st.norma : d.norma;
  static const std::vector<Noc> pusta;
  const std::vector<Noc>& h = d.hist ? *d.hist : pusta;
  const Noc* k = ostatniaNoc(h, d.dzis);
  t.tytul = k ? tytulNocy(*k, st) : std::string("Brak zapisanych nocy.");  // karta K22
  std::string koniec = k ? k->data : d.dzis;
  // kolumny: mediana 7 nocy, strzałka i słowo (5.4.2; w trybie ASV `EPAP` zamiast ciśnienia, R-KOK-15, 4.11); jednostka
  // wycieku w nazwie kolumny (wariant B)
  Porownanie c = porownajTydzien(wartosci7(h, koniec, poleM01), nm.m01, Strona::WYZEJ_LEPIEJ);
  Porownanie a = porownajTydzien(wartosci7(h, koniec, poleAhi), nm.ahi, Strona::NIZEJ_LEPIEJ);
  // trzecia kolumna według trybu bieżącego: CPAP i AutoSet `ciśnienie` (M-05, mediana), ASV i ASVAuto `EPAP` (M-06);
  // tylko noce w trybie bieżącym (noc bez znanego trybu liczy się, jak w normie R-NOR-07); bez strony lepszej (ADR-0002)
  reguly::Tryb tryb = trybBiezacy(h, koniec);  // ta sama reguła co kokpit (src/kokpit.h)
  bool cisnienie = tryb == reguly::Tryb::CPAP || tryb == reguly::Tryb::AUTOSET;
  // tryb nocy jak w kokpicie (trybNocy() z src/noc.h; noc z karty bez trybu ma tryb ostatniej znanej, noc z AirBridge
  // bez trybu nie liczy się, gdy tryb bieżący jest znany, R2)
  std::vector<Noc> wTrybie;
  HistoriaPos hp = posortujHistorie(h);
  reguly::Tryb znany = reguly::Tryb::NIEZNANY;
  for (const Noc* n : hp.noce) {
    std::string tt = trybNocy(*n);
    reguly::Tryb tn = tt.empty() ? (n->zrodlo == 'K' ? znany : reguly::Tryb::NIEZNANY) : reguly::trybZUstawien(tt);
    if (!tt.empty()) znany = tn;
    bool obcy = tt.empty() && n->zrodlo != 'K';
    if (tryb == reguly::Tryb::NIEZNANY || (!obcy && tn == tryb)) wTrybie.push_back(*n);
  }
  Porownanie e = cisnienie ? porownajTydzien(wartosci7(wTrybie, koniec, poleCmedDnia), nm.cmed, Strona::BEZ_OCENY)
                           : porownajTydzien(wartosci7(wTrybie, koniec, poleEmed), nm.emed, Strona::BEZ_OCENY);
  Porownanie w = porownajTydzien(wartosci7(h, koniec, poleWp95), nm.wp95, Strona::NIZEJ_LEPIEJ);
  t.kolumny = {
      // nie „Sen”: aparat nie mierzy snu (A-65); mediana `6:39` celowo innym zapisem niż noc `6 h 39` nad paskiem,
      // żeby się nie myliły (5.4.2, SPEC-B)
      kolumna("czas z maską", godzMin(c.mediana), c.slowo, c.spokojne),
      kolumna("AHI", przecinek1(a.mediana), a.slowo, a.spokojne),  // jedna nazwa M-02
      kolumna(cisnienie ? "ciśnienie" : "EPAP", przecinek1(e.mediana), e.slowo, e.spokojne),
      kolumna("wyciek l/min", brak(w.mediana) ? std::string("–") : calkowita(w.mediana), w.slowo, w.spokojne),
  };
  bool spokojny = c.spokojne && a.spokojne && e.spokojne && w.spokojne;
  blokNocy(t, d, k, st, spokojny ? "Tydzień jak zwykle." : "Tydzień: różnice zaznaczone niżej.",
           spokojny ? "Tydzień jak zwykle." : "Tydzień: inaczej niż zwykle.");
  // karta K22: rząd faktów ostatniej nocy zamiast paska (R-HUM-13); jedyne miejsce liczb nocy na karcie; bez `P95`
  // na ekranach dnia i kartach
  if (k) t.fakty = faktyK22(*k, t.tytul, {calkowita(k->wp95), "wyciek l/min"});
  return t;
}

static TrescEkranu s2b(const DaneDnia& d) {
  TrescEkranu t;
  t.uklad = Uklad::SERIA;
  t.etykieta = "SERIA";
  int n = seria(*d.hist, d.dzis);
  t.seria = n;
  t.tytul = std::to_string(n) + " " + nocyZRzedu(n) + " z rzędu z maską.";  // karta K01 (ten sam tekst, R-HUM-13)
  int k30 = 0;
  for (int i = 0; i < 30; i++) {
    const Noc* r = nocZDnia(*d.hist, dodajDni(d.dzis, i - 29));
    t.kropka[i] = r && r->m01 >= 240;
    k30 += t.kropka[i];
  }
  const Noc* r = nocZDnia(*d.hist, d.dzis);
  t.fakty = {{r ? godzH(r->m01) : "–", "ostatnia noc z maską"}, {std::to_string(k30) + " z 30", "nocy ponad 4 h"}};
  return t;
}

// S2c (5.4.2, artboard S2-sypialnia): temperatura i wilgotność z czujnika panelu, CO2 sypialni (mediana nocy z lokalnego
// źródła otoczenia, ADR-0019) jako trzecia kolumna tylko, gdy ostatnia noc ma odczyt CO2 (R-EKR-17); PM2.5 sypialni
// nie ma w kontrakcie wersji 1. Powód: propozycja; CO2 bez normy (`uczę się`) nie zmienia zdania, ale mediana CO2
// nocy na progu S-K-01 (CO2_PROG_NOCY, 1000 ppm) albo wyżej nigdy nie daje `Dobre warunki do snu.`: kolumna CO2 ma wtedy
// czarne pole „ponad próg” (także bez normy), a zdanie mówi o sypialni. Różnica tylko
// w CO2 (czujnik sypialni, nie przy łóżku): `Sypialnia: różnice niżej.`
// Odczyty z czujnika w obudowie panelu, czyli przy łóżku (M-22), nie z czujnika sypialni: pasek `Przy łóżku · ostatnia
// noc` (`sypialnia` tylko dla Qingping, M-23).
static TrescEkranu s2c(const DaneDnia& d) {
  TrescEkranu t;
  t.uklad = Uklad::TYDZIEN;
  t.etykieta = "PRZY ŁÓŻKU · OSTATNIA NOC";
  t.etykietaKarty = "OSTATNIA NOC";
  t.tytulPaska = "Przy łóżku · ostatnia noc";
  Stany zapas;
  const Stany& st = stanyDnia(d, zapas);
  static const std::vector<Noc> pusta;
  const Noc* k = ostatniaNoc(d.hist ? *d.hist : pusta, d.dzis);
  t.tytul = k ? tytulNocy(*k, st) : std::string("Brak zapisanych nocy.");
  float tv = d.sypNoc ? d.sypNoc->t : NAN, wv = d.sypNoc ? d.sypNoc->w : NAN;
  bool s1 = false, s2 = false;
  std::string sl1 = slowoSypialni(tv, d.sypNorma.t, s1, "°"), sl2 = slowoSypialni(wv, d.sypNorma.w, s2, "%");
  // poza pasmem: `↑ zwykle 21,5°` → strzałka i słowo `zwykle 21,5°`, kolumna w ramce (wariant B)
  t.kolumny = {kolumna("temperatura", brak(tv) ? std::string("–") : przecinek1(tv) + "°", sl1, s1),
               kolumna("wilgotność", brak(wv) ? std::string("–") : calkowita(wv) + "%", sl2, s2)};
  bool spokojCo2 = true;
  float cv = d.sypNoc ? d.sypNoc->co2 : NAN;
  if (!brak(cv)) {
    bool s3 = false;
    std::string sl3 = slowoSypialni(cv, d.sypNorma.co2, s3, "");
    Kolumna kc = kolumna("CO2 ppm", calkowita(cv), sl3, s3);
    if (cv >= CO2_PROG_NOCY) {  // ponad próg: czarne pole zamiast ramki, słowo z progiem
      kc.ponadProg = true;
      kc.poza = false;
      kc.p = "ponad " + std::to_string(CO2_PROG_NOCY);
      if (kc.s.empty() || kc.s == "↔") kc.s = "↑";
      spokojCo2 = false;
    } else {
      spokojCo2 = s3 || !d.sypNorma.co2.jest();
    }
    t.kolumny.push_back(kc);
  }
  bool spokojLozko = s1 && s2, spokoj = spokojLozko && spokojCo2;
  bool ponadProgCo2 = !t.kolumny.empty() && t.kolumny.back().ponadProg;
  // krótsze zdanie niż S2a (`Noc w normie. Przy łóżku: różnice zaznaczone niżej.` nie miało zapasu w 744 px).
  // Karta K22 (bez paska): przy samym przekroczeniu progu CO2 `Sypialnia: CO2 ponad 1000.`, `inaczej niż zwykle` tylko
  // przy CO2 poza pasmem normy
  std::string karta = spokoj        ? "Dobre warunki do snu."
                      : !spokojLozko ? "Przy łóżku: inaczej niż zwykle."
                      : ponadProgCo2 ? "Sypialnia: CO2 ponad " + std::to_string(CO2_PROG_NOCY) + "."
                                     : "Sypialnia: inaczej niż zwykle.";
  blokNocy(t, d, k, st,
           spokoj ? "Dobre warunki do snu." : spokojLozko ? "Sypialnia: różnice niżej." : "Przy łóżku: różnice niżej.",
           karta);
  if (k) t.fakty = faktyK22(*k, t.tytul, {brak(tv) ? std::string("–") : calkowita(tv) + "°", "przy łóżku"});
  return t;
}

static TrescEkranu kamien(const DaneDnia& d, int n) {
  TrescEkranu t;
  t.etykieta = "KAMIEŃ MILOWY";
  t.tytul = std::to_string(n) + " nocy z maską.";  // karta
  t.duza = std::to_string(n);  // ekran danych: duża liczba (5.8.3, wariant B)
  t.podpisDuzej = "nocy z maską";
  std::string pierwsza;
  long minuty = 0;
  std::vector<double> ahi;
  for (auto& r : *d.hist)
    if (r.data <= d.dzis) {
      if (pierwsza.empty() || r.data < pierwsza) pierwsza = r.data;
      if (r.m01 > 0) minuty += r.m01;
      if (!brak(r.ahi)) ahi.push_back(r.ahi);
    }
  // bez powtórzenia `{n} nocy z maską` (stoi w tytule i dużej liczbie), data z rokiem
  t.powod = "Pierwsza noc: " + dataZRokiem(pierwsza) + ". Każda się liczy.";
  int cel = 0;
  for (int k : KAMIENIE)
    if (k > n) { cel = k; break; }
  t.krok = cel ? "→ Następny cel: " + std::to_string(cel) + " nocy." : std::string("→ Następny cel: kolejny rok.");
  t.fakty = {{std::to_string((minuty + 30) / 60) + " h", "z maską"}, {przecinek1(mediana(ahi)), "AHI, mediana"}};
  return t;
}

// Noce bez danych do dziś włącznie (licznik E-01, R-AW-24, R-AW-32): 1 = tylko dziś. To samo liczenie co plan
// wybudzeń (src/noc.h nocyBezDanychZRzedu()); dla ekranu najmniej 1 (S5 stoi tylko przy E-01).
int nocyBezDanych(const DaneDnia& d) {
  return std::max(1, nocyBezDanychZRzedu(d.dzis, d.abData, d.abMinuta, d.bladData, d.hist));
}

// Data „od” w powodzie E-01 od 3. nocy: dzień ostatniego udanego odczytu, a bez niego dzień przed pierwszą brakującą
// nocą; z rokiem, gdy to inny rok niż dziś.
static std::string odKiedyBrak(const DaneDnia& d, int n) {
  std::string od = dataOk(d.abData) ? d.abData : dodajDni(d.dzis, -n);
  return od.substr(0, 4) == d.dzis.substr(0, 4) ? ddmm(od) : dataZRokiem(od);
}

// S10 (R-EKR-57): noc jeszcze w drodze, przed 11:00 albo w budowie. Ekran przejściowy, karta z (K23).
static TrescEkranu s10(const DaneDnia& d) {
  TrescEkranu t;
  t.etykieta = "CZEKAM NA DANE";
  t.tytul = "Synchronizuję się z aparatem.";
  t.powod = "Noc " + ddmm(d.dzis) + " zaraz tu będzie. Zaglądam co\xC2\xA0" "30\xC2\xA0" "min.";
  t.stopkaLewo = "▶ kokpit";  // podpis każdej akcji (5.6.1 pkt 6)
  t.stopkaPrawo = "↻ pobierz teraz";  // ↻ ponawia pobranie jak na S5 (R-PRZ-06)
  return t;
}

// S5 (6.4): teksty dokładnie z katalogu zdarzeń, bez kodów technicznych (kod HTTP, powód Wi-Fi i 401 są w logu UART
// i na ekranie testu). Brak Wi-Fi, brak odpowiedzi AirBridge i odrzucone logowanie (401) to dla użytkownika ten
// sam stan E-01; E-03 tylko przy nieczytelnym formacie (R-AW-02) albo wersji AirBridge spoza zakresu (Załącznik A).
static TrescEkranu s5(const DaneDnia& d) {
  TrescEkranu t;
  t.etykieta = "BRAK DANYCH Z NOCY";
  if (!d.e05Tytul.empty()) {  // E-05 krytyczne: bateria ekranu, nie dane z nocy (etykieta spoza 6.4)
    t.etykieta = "BATERIA EKRANU";
    t.tytul = d.e05Tytul;
    t.powod = d.e05Powod;
    t.krok = d.e05Krok;
  } else if (d.httpKod == KOD_E03) {
    t.tytul = "Nie rozumiem danych AirBridge";
    t.powod = d.abWersja.empty() ? "Format raportu zmienił się." : "Format raportu zmienił się (AirBridge " + d.abWersja + ").";
    t.krok = "→ Daj znać osobie, która instalowała panel.";
  } else {  // E-01: łagodnie przez 2 noce, „awaria” z krokiem dopiero od 3. nocy bez danych (R-AW-24, R-AW-32)
    int n = nocyBezDanych(d);
    // twarda spacja w `co 30 min.` (R-EKR-08): przy zawijaniu `co` nie zostaje samo na końcu linii
    const std::string co30 = "co\xC2\xA0" "30\xC2\xA0" "min.";
    if (n == 1) {
      t.etykieta = "CZEKAM NA DANE";
      t.tytul = "Noc " + ddmm(d.dzis) + " jeszcze w drodze.";
      // {godzina}: ostatni udany odczyt, a bez niego pierwsza nieudana próba (od kiedy AirBridge milczy)
      std::string od = !d.ostatnieDane.empty() ? d.ostatnieDane : d.bladOd;
      t.powod = od.empty() ? "AirBridge na razie milczy. Pytam " + co30 : "AirBridge milczy od " + od + ". Pytam " + co30;
    } else {
      // od 2. nocy bez odpowiedzi panel po 11:00 pyta co 3 h (R-DANE-18, src/plan.h), więc `Sprawdzam co 3 h.`
      // (R-AW-24); przy odrzuconym logowaniu dalej co 30 min (R-AW-34), więc `Ponawiam co 30 min.` (R-AW-35)
      const std::string rytm = d.logowanieOdrzucone ? "Ponawiam " + co30 : "Sprawdzam co\xC2\xA0" "3\xC2\xA0" "h.";
      if (n == 2) {
        t.etykieta = "CZEKAM NA DANE";
        t.tytul = "Brak danych od 2 nocy";
        t.powod = "Wyjazd z aparatem? Dane wrócą same. " + rytm;
      } else {
        t.tytul = "Brak danych od " + std::to_string(n) + " nocy";
        t.powod = "AirBridge nie odpowiada od " + odKiedyBrak(d, n) + ". " + rytm;
        t.krok = "→ Sprawdź AirBridge i sieć. Ustawienia: strona 6 / 6.";
      }
    }
  }
  // E-01: godzina ostatnich danych stoi już w powodzie (`nie odpowiada od {godzina}`), więc bez drugiej godziny
  // E-03 nie ma godziny w powodzie: linia faktu (tekst spoza 6.4, SPEC-B 8)
  if (d.e05Tytul.empty() && d.httpKod == KOD_E03 && !d.ostatnieDane.empty()) {
    t.fakty = {{d.ostatnieDane, "ostatnie dane"}};  // karta (E-03 jest zawsze ekranem danych, R-HUM-08)
    t.fakt = "ostatnie dane: " + d.ostatnieDane;
  }
  t.ramkaPrzerywana = true;
  t.stopkaLewo = "▶ kokpit";
  t.stopkaPrawo = "↻ ponów";  // S5: ↻ ponawia pobranie (R-PRZ-06), akcja niestandardowa (R-PRZ-04)
  // na S5 baterii ↻ nie pobiera, tylko mierzy baterię od nowa i przerysowuje ekran dnia (jak `▶ odśwież` U-05)
  if (!d.e05Tytul.empty()) t.stopkaPrawo = "↻ odśwież";
  return t;
}

static TrescEkranu s6(const DaneDnia& d) {
  TrescEkranu t;
  t.uklad = Uklad::ROZRUCH;
  t.etykieta = "PIERWSZE DNI";  // artboard S6-rozruch
  t.tytul = "Uczę się twojej normy.";
  const Noc* k = d.hist ? nocZDnia(*d.hist, d.dzis) : nullptr;
  int wszystkie = d.hist ? liczbaNocyWaznych(*d.hist, d.dzis) : 0;
  // R-EKR-45: pierwszy dzień rozruchu zdanie ogólne, potem kolejny wariant opisu ostatniej nocy. Warianty z M-18
  // (start), M-17 (przerwy) i M-13 (oddechy) mają w rekordzie nocy wartość `null`, więc panel je pomija: zostaje wariant 3.
  if (wszystkie > 1 && k && k->m01 >= 0 && !brak(k->wp95))
    t.powod = "Ostatnia noc: " + godzH(k->m01) + " z maską, wyciek " + calkowita(k->wp95) + " l/min.";  // bez P95
  else
    t.powod = "Do tego czasu pokazuję tylko sprawy, które nie wymagają normy.";
  // postęp liczony z nocy w oknie normy (90 dni); gdy nocy ważnych jest dość, ale żadnej w oknie, to nie
  // „noc 200 z 14”, tylko brak nowych nocy
  int n = d.norma.n < 14 ? d.norma.n : 13;
  t.postep = n;
  if (wszystkie >= 14 && n == 0) {
    t.postepLewo = "brak nowych nocy";
    t.postepPrawo = "norma po 14 nocach";
  } else {
    t.postepLewo = "noc " + std::to_string(n) + " z 14";
    t.postepPrawo = "pełna norma od ok. " + ddmm(dodajDni(d.dzis, 14 - n));
  }
  return t;
}

TrescEkranu trescDnia(const std::string& typ, const std::string& klucz, const DaneDnia& d) {
  TrescEkranu t;
  int n = 0;
  if (typ == "S5") t = s5(d);
  else if (typ == "S10") t = s10(d);
  else if (typ == "S6") t = s6(d);
  else if (typ == "KM" && liczbaKlucza(klucz, n)) t = kamien(d, n);  // uszkodzony klucz z `/humor.csv`: S2a niżej
  else if (typ == "S2B") t = s2b(d);
  else if (typ == "S2C") t = s2c(d);
  else t = s2a(d);
  t.linia = d.linia;
  t.liniaA = d.liniaA;
  t.liniaOstrzezenie = d.liniaOstrzezenie;
  if (t.stopkaPrawo.empty()) {  // akcje standardowe: pogoda i odczyt czujnika panelu (R-EKR-10, R-HUM-18)
    t.stopkaStandardowa = true;
    t.stopkaLewo = d.pogoda;
    t.stopkaPrawo = d.przyLozku;
    dopasujStopke(t.stopkaLewo, t.stopkaPrawo);
  }
  return t;
}

void dopasujStopke(std::string& lewo, std::string& prawo) {
  auto miesci = [&]() {
    return lewo.empty() || prawo.empty() || szer(F_P22B, lewo) + szer(F_P22B, prawo) + ODSTEP_STOPKI <= SZER_STOPKI;
  };
  if (miesci()) return;
  size_t i = lewo.find(" · ");  // 1. bez opisu pogody: `Katowice 8°`
  if (i != std::string::npos) lewo = lewo.substr(0, i);
  if (miesci()) return;
  size_t sp = lewo.rfind(' ');  // 2. bez miasta: `8°`
  if (sp != std::string::npos) lewo = lewo.substr(sp + 1);
  if (miesci()) return;
  size_t c = prawo.find(" · CO2 ");  // 3. bez CO2: `przy łóżku 21° · 45%`
  if (c != std::string::npos) prawo = prawo.substr(0, c);
}

std::string tekstE08(const std::string& dataOd, int minutaOd, const std::string& dzis) {
  std::string kiedy = kiedyOdczyt(dataOd, minutaOd, dzis);
  return kiedy.empty() ? "Brak odczytu CO2." : "Brak odczytu CO2 od " + kiedy + ".";
}

bool liniaJestOstrzezeniem(GrupaLinii g) {
  switch (g) {
    case GrupaLinii::PILNOSC_A_SENNOSC:
    case GrupaLinii::WZORCE_L:
    case GrupaLinii::PRZYPOMNIENIE_R5B:
    case GrupaLinii::NOC_R_SKI_24: return true;
    default: return false;  // AWARIE nigdy nie są ostrzeżeniem
  }
}

std::string etykietaPilnosci(reguly::Pilnosc p) {
  switch (p) {
    case reguly::Pilnosc::A: return "Skontaktuj się w ciągu kilku dni";
    case reguly::Pilnosc::B: return "Warto skonsultować · w najbliższych tygodniach";
    default: return "";
  }
}

namespace {
float wartoscWzorca(const std::string& id, const Noc& n) {
  // tylko wzorce z ekranem w 1.0 i wykresem liniowym w 5.13.4; L-01 (słupki obturacyjne/centralne), L-02 (słupki
  // minut), L-04 i L-12 działają w 1.0 w stanie „tylko raport” (bez ekranu, ADR-0029)
  if (id == "L-03") return n.ahi;
  if (id == "L-09") return n.wp95;
  return NAN;
}
const Noc* nocDnia(const std::vector<Noc>& h, const std::string& data) {
  for (auto& n : h)
    if (n.data == data) return &n;
  return nullptr;
}
}  // namespace

DaneWzorca trescWzorca(const sprawy::SprawaL& s, const DaneDnia& d) {
  if (s.jestE()) return zdarzenia::trescZdarzenia(s, d);  // S3 zdarzenia E (src/ekrany_zdarzen.h)
  DaneWzorca w;
  if (s.ton == sprawy::Ton::PROBLEM) {  // L-09 to ekran danych dnia ze strefą linii uwagi (5.4.3)
    w.linia = d.linia;
    w.liniaA = d.liniaA;
    w.liniaOstrzezenie = d.liniaOstrzezenie;
  }
  w.naglowek = {naglowekDniaLewo(d), naglowekDniaPrawo(d)};
  w.lekarz = s.ton != sprawy::Ton::PROBLEM;
  w.etykieta = w.lekarz ? etykietaPilnosci(s.pilnosc) : "";
  w.tytul = s.tytul;
  w.powod = s.powod;
  w.krok = s.krok;
  w.pilnoscA = s.pilnosc == reguly::Pilnosc::A;
  w.hipoteza = s.hipoteza;
  w.podpisLewo = s.podpisLewo;
  w.podpisPrawo = s.podpisPrawo;
  if (!d.hist || !dataOk(d.dzis)) return w;
  if (s.id == "L-08") {  // 30 nocy wobec 30 nocy miesiąc wcześniej (5.13.4, wyjątek od 14 nocy)
    w.wiz = WizWzorca::RZEDY;
    w.etRzad1 = s.rzad1;
    w.etRzad2 = s.rzad2;
    for (int i = 0; i < 30; i++) {
      const Noc* a = nocDnia(*d.hist, dodajDni(d.dzis, i - 59));
      const Noc* b = nocDnia(*d.hist, dodajDni(d.dzis, i - 29));
      w.rzad1[i] = a && a->m01 >= 240;
      w.rzad2[i] = b && b->m01 >= 240;
    }
    return w;
  }
  bool jest = false;
  for (int i = 0; i < 14; i++) {
    const Noc* n = nocDnia(*d.hist, dodajDni(d.dzis, i - 13));
    float v = n ? wartoscWzorca(s.id, *n) : NAN;
    jest = jest || !std::isnan(v);
    w.wartosci.push_back(v);
  }
  if (jest) {
    w.wiz = WizWzorca::LINIA;
    w.prog = (float)s.progLinii;
  } else {
    w.wartosci.clear();
  }
  return w;
}

DanePotwierdzenia trescPrzeniesione(const sprawy::SprawaL& s, const DaneDnia& d, bool cofniecie) {
  DanePotwierdzenia p;
  p.naglowek = {naglowekDniaLewo(d), naglowekDniaPrawo(d)};
  p.tytul = "Przeniesione na " + std::to_string(sprawy::dniWiem(s)) + " dni.";
  p.powod = "Do tego czasu przypomnę na pasku u góry: " + s.tytul + ".";
  if (cofniecie) p.stopka = {"◀ 2 s cofnij"};  // cofnięcie zawsze przytrzymaniem ◀
  return p;
}

DanePotwierdzenia trescZapisano(sprawy::Pytanie q, bool tak, const DaneDnia& d, bool cofniecie) {
  DanePotwierdzenia p;
  p.naglowek = {naglowekDniaLewo(d), naglowekDniaPrawo(d)};
  p.nad = trescPytaniaTN(q, d, -1, 0).tytul;
  p.tytul = tak ? "Zapisano: tak." : "Zapisano: nie.";
  if (cofniecie) p.stopka = {"◀ 2 s cofnij"};  // ten sam podpis cofnięcia na wszystkich potwierdzeniach
  return p;
}

DaneRaportu trescRaportu(const sprawy::SprawaL& s, const DaneDnia& d) {
  DaneRaportu r;
  r.naglowek = {naglowekDniaLewo(d), ""};
  r.temat = s.tytul;
  r.kod = KOD_RAPORTU_BRAK;
  return r;
}

DaneOmowione trescOmowione(const sprawy::SprawaL& s, const std::string& dzis, int wariant) {
  DaneOmowione o;
  std::string dz = dataOk(dzis) ? std::string(DNI_KROTKO[dzienTygodnia(dzis)]) + " " + ddmm(dzis) : "";
  o.naglowek = {dz + " · po wizycie", "przytrzymano ◀ 2 s"};
  o.nazwa = s.tytul;
  o.zdania = wariant < 0 ? zdaniaOmowione(s.pilnosc == reguly::Pilnosc::A) : zdaniaOmowienia(wariant);
  o.blokada = wariant == 5;  // NAJPIERW_RAPORT (R-LEK-26)
  return o;
}

DaneSpraw trescSpraw(const std::vector<sprawy::KartaSprawy>& karty, int i, const sprawy::WejscieSpraw& w) {
  DaneSpraw d;
  d.naglowek = "Otwarte sprawy";
  if (dataOk(w.dzis)) d.naglowek += std::string(" · ") + DNI_KROTKO[dzienTygodnia(w.dzis)] + " " + ddmm(w.dzis);
  sprawy::KartaSprawy k;
  if (karty.empty()) {
    k = sprawy::kartaPusta(w);
    d.i = d.n = 0;
  } else {
    d.n = (int)karty.size();
    d.i = std::max(1, std::min(i, d.n));
    k = karty[(size_t)(d.i - 1)];
  }
  d.tytul = k.tytul;
  d.powod = k.powod;
  d.ocena = k.ocena;
  d.ty = k.ty;
  d.dalej = k.dalej;
  d.koniec = k.koniec;
  d.dopisek = k.dopisek;
  return d;
}

DanePytaniaTN trescPytaniaTN(sprawy::Pytanie p, const DaneDnia& d, int termin, int nocyPrzerwy) {
  DanePytaniaTN t;
  std::string hh = termin >= 0 ? (termin / 60 < 10 ? "0" : "") + std::to_string(termin / 60) + ":" +
                                     (termin % 60 < 10 ? "0" : "") + std::to_string(termin % 60)
                               : "11:00";
  switch (p) {
    case sprawy::Pytanie::SENNOSC:  // 5.12.8, brzmienie po przeglądzie klinicznym
      t.naglowek = {naglowekDniaLewo(d) + " · tydzień", ""};
      t.etykieta = "Jedno pytanie, raz w tygodniu";
      t.tytul = "Zasnąłeś w dzień mimo woli?";
      t.opis = "W ostatnim tygodniu, gdy chciałeś nie spać.";
      t.nadPolami = "odpowiedź trafia do raportu";
      t.stopka = "Bez odpowiedzi przez 30 min = brak, nie „nie”.";
      break;
    case sprawy::Pytanie::GORZEJ:
      t.naglowek = {naglowekDniaLewo(d), naglowekDniaPrawo(d)};
      t.etykieta = "Pytanie poranne";
      t.tytul = "Czujesz się gorzej niż zwykle?";
      t.opis = "Odpowiedź uwzględnię w ocenie ostatnich nocy.";
      t.stopka = "Bez odpowiedzi do " + hh + " = brak, nie „nie”.";
      break;
    case sprawy::Pytanie::PRZERWA:
      t.naglowek = {naglowekDniaLewo(d), naglowekDniaPrawo(d)};
      t.etykieta = "Po " + std::to_string(nocyPrzerwy) + " nocach przerwy";
      t.tytul = "Przerwa była planowana?";
      t.opis = "Np. urlop albo zabieg bez aparatu.";
      t.stopka = "Bez odpowiedzi do " + hh + " = brak, nie „nie”.";
      break;
    case sprawy::Pytanie::INFEKCJA:  // S-K-30 (5.10.2); ekran rysuje src/kontekst_sk.h, tu nad „Zapisano”
      t.naglowek = {naglowekDniaLewo(d), naglowekDniaPrawo(d)};
      t.etykieta = "Rano · noc inna niż zwykle";
      t.tytul = "Katar albo infekcja?";
      break;
    case sprawy::Pytanie::PODUSZKA:  // R-MAS-12 (5.9.5, artboard Q-poduszka); opis z liczbami ustawia dyrygent
      t.naglowek = {naglowekDniaLewo(d), naglowekDniaPrawo(d)};
      t.etykieta = "Sprzęt";
      t.tytul = "Wymieniona maska?";
      t.stopka = "Bez odpowiedzi do " + hh + " = nie zapisuję wymiany.";
      break;
    case sprawy::Pytanie::WYJAZD:  // R-CZY-22: teksty z okresu (czynniki.h tekstyWyjazdu(), dyrygent)
      t.naglowek = {naglowekDniaLewo(d), naglowekDniaPrawo(d)};
      break;
  }
  return t;
}
