// SPDX-License-Identifier: GPL-3.0-or-later
#include "cykl.h"
#include "nawigacja.h"
#include "wspolne.h"

bool oknoDnia(int minuta) { return !slotNocny(minuta); }

bool nocZnana(const WejscieDnia& w) {
  if (!dataOk(w.dzis)) return false;
  if (w.hist && nocZDnia(*w.hist, w.dzis)) return true;
  return w.abData == w.dzis && w.abMinuta >= 5 * 60;
}

bool brakDanych(const WejscieDnia& w) { return brakDanychAirBridge(w) || (w.e05Krytyczne && dataOk(w.dzis)); }

bool brakDanychAirBridge(const WejscieDnia& w) {
  if (w.e03 && dataOk(w.dzis) && !(w.hist && nocZDnia(*w.hist, w.dzis))) return true;  // R-AW-02: E-03 na ekran
  if (w.odczytOk || nocZnana(w)) return false;
  // przed 11:00 w oknie dnia: noc z dziś może jeszcze przyjść (S10, R-EKR-57), chyba że brak danych trwa już od
  // poprzedniej nocy (E-01 z wczoraj nie skończyło się, 6.4): wtedy S5 także rano, bez „zaraz tu będzie”
  if (oknoDnia(w.minuta) && w.minuta < 11 * 60) {
    bool wczorajJest = w.hist && nocZDnia(*w.hist, dodajDni(w.dzis, -1));
    return !wczorajJest && nocyOdOdczytu(w.dzis, w.abData, w.abMinuta) >= 2;
  }
  return true;
}

bool synchronizacja(const WejscieDnia& w) {
  if (!dataOk(w.dzis) || !oknoDnia(w.minuta) || w.terapia || w.e03) return false;
  if (w.hist && nocZDnia(*w.hist, w.dzis)) return false;
  if (brakDanych(w)) return false;
  if (w.minuta < 11 * 60) return true;
  // po 11:00 tylko, gdy AirBridge powiedział dziś, że noc jest w budowie; jedno nieudane wybudzenie (stan w RTC,
  // noc znana ze statusu) nie przerywa S10 i nie robi wpisu w dzienniku na nocy z wczoraj
  return w.wBudowie && (w.odczytOk || nocZnana(w));
}

StanDnia stanDnia(const WejscieDnia& w) {
  StanDnia s;
  s.dzis = w.dzis;
  s.brakDanych = brakDanych(w);
  s.kluczBraku = w.kluczBraku;
  s.kartaBraku = w.kartaBraku;
  if (s.brakDanych && !brakDanychAirBridge(w)) {  // S5 z E-05 krytycznym: zawsze ekran danych (R-HUM-08)
    s.kluczBraku = "E-05";
    s.kartaBraku = false;
  }
  s.synchronizacja = synchronizacja(w);
  const Noc* nd = w.hist ? nocZDnia(*w.hist, w.dzis) : nullptr;
  s.nocDzis = nd;
  s.nocDzisPonad4h = nd && nd->m01 >= 240;
  if (w.hist) {
    s.nocyWaznych = liczbaNocyWaznych(*w.hist, w.dzis);
    s.seria = seria(*w.hist, w.dzis);
    s.nocyPonad4h = nocyPonad4h(*w.hist, w.dzis);
  }
  s.maNorme = w.maNorme;
  s.sypialnia = w.sypialnia;
  return s;
}

DecyzjaDnia decyzjaDnia(const WejscieDnia& w, const std::vector<WpisHumoru>& log) {
  DecyzjaDnia d;
  if (!oknoDnia(w.minuta)) {
    d.zapisz = false;
    // slot nocny: ostatni ekran dnia z dziennika (ten, który stał wieczorem), bez nowego wpisu
    for (auto& x : log)
      if (x.typ != "S11" && (!d.wpis || x.data >= d.wpis->data)) d.wpis = &x;  // Portret tylko rano (R-POR-20)
    if (d.wpis) {
      d.e.typ = d.wpis->typ;
      d.e.klucz = d.wpis->klucz;
      d.dataEkranu = d.wpis->data;
    } else {
      d.e.typ = "S2A";
      d.dataEkranu = w.minuta < 5 * 60 ? dodajDni(w.dzis, -1) : w.dzis;
    }
    // E-05 krytyczne w slocie nocnym to S5 baterii (zostaje do 05:00, N0, R-PIE-02), także gdy bateria
    // spadła po ostatnim rysowaniu wieczorem: bez S9 (ekranDniaTrwa()) i bez ekranu z dziennika, który o baterii milczy
    if (brakDanych(w) && !brakDanychAirBridge(w) && !(d.e.typ == "S5" && d.e.klucz == "E-05")) {
      d.e = EkranDnia();
      d.e.typ = "S5";
      d.e.klucz = "E-05";
      d.wpis = nullptr;
    }
    return d;
  }
  d.nowyWybor = true;
  d.e = wybierzEkranDnia(stanDnia(w), log);
  d.dataEkranu = w.dzis;
  d.wpis = znajdzWpis(log, w.dzis, d.e.typ, d.e.klucz);
  // Dowód spokoju zależy od ostatniej nocy (seria, rotacja), a dzień zostaje przy pierwszym wyborze. Dopóki nocy
  // z dziś nie ma w historii, a do 11:00 [PRÓG] może jeszcze przyjść (4.1: brak przed 11:00 = „jeszcze nie ma”),
  // wybór jest tymczasowy: ekran danych bez wpisu w dzienniku, bez karty i bez przesuwania rotacji.
  if (d.e.typ == "S10") {  // ekran przejściowy: bez wpisu w dzienniku, poza limitami kart i rotacją (R-EKR-57)
    d.zapisz = false;
    return d;
  }
  bool spokoj = d.e.typ == "S2A" || d.e.typ == "S2B" || d.e.typ == "S2C";
  bool nocWHistorii = w.hist && nocZDnia(*w.hist, w.dzis);
  if (spokoj && !d.wpis && !nocWHistorii && w.minuta < 11 * 60) {
    bool dzisWpis = false;
    for (auto& x : log)
      if (x.data == w.dzis && (x.typ == "S2A" || x.typ == "S2B" || x.typ == "S2C")) dzisWpis = true;
    if (!dzisWpis) {
      d.zapisz = false;
      d.e.kartaZTabeli = false;
    }
  }
  return d;
}

bool przerysowacPoZegarze(int minuta) { return oknoDnia(minuta); }

bool poranekCzynnikow(std::vector<WpisCzynnikow>& cz, const std::vector<Noc>& hist, const std::string& dzis, int minuta,
                      const Norma& norma, uint32_t ziarno, const OkolicznosciRanka& ok) {
  if (!dataOk(dzis)) return false;
  bool zmiana = zamknijPoTerminie(cz, dzis, minuta);
  if (!oknoDnia(minuta)) return zmiana;
  const Noc* n = nocZDnia(hist, dzis);
  if (!n || wpisZDnia(cz, dzis)) return zmiana;
  WejscieDecyzji we;
  we.noc = n;
  we.norma = norma;
  we.nocyWaznychPrzed = liczbaNocyWaznych(hist, dodajDni(dzis, -1));
  we.ziarno = ziarno;
  we.prosbaNaEkranie = ok.prosbaNaEkranie;  // R-PIE-10, R-CZY-17
  we.wzorzecA = ok.wzorzecA;                // R-CZY-01, R-PIE-06
  we.sk30 = ok.sk30;                        // R-CZY-15
  we.sk17 = ok.sk17;                        // R-CZY-07
  char war = decyzjaZaproszenia(we);
  if (!war) return zmiana;
  WpisCzynnikow c;
  c.data = dzis;
  c.stan = 'C';
  c.warstwa = war;
  c.termin = terminOdpowiedzi(minuta);
  cz.push_back(c);
  return true;
}
