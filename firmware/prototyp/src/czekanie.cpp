// SPDX-License-Identifier: GPL-3.0-or-later
// Czekanie na zasilaniu zewnętrznym zamiast głębokiego snu (src/czekanie.h, ADR-0047).
#include "czekanie.h"

#include "wspolne.h"  // nrDnia()

namespace czekanie {

Decyzja decyzja(const Wejscie& w) {
  Decyzja d;
  d.wybor = Wybor::SEN;
  if (!w.obsluga) d.powod = Powod::BEZ_OBSLUGI;
  else if (w.awaryjny) d.powod = Powod::AWARYJNY;
  else if (w.naStarcie != Zasilanie::ZEWNETRZNE || w.przedSnem != Zasilanie::ZEWNETRZNE) d.powod = Powod::NIE_KABEL;
  else if (w.wylaczone) d.powod = Powod::WYLACZONE;
  else if (w.ustawienieNieznane) d.powod = Powod::USTAWIENIE_NIEZNANE;
  else if (w.nvsNowszy) d.powod = Powod::NVS_NOWSZY;
  else if (w.poBledzie) d.powod = Powod::PO_BLEDZIE;
  else if (!w.budzZegarem) d.powod = Powod::BEZ_BUDZIKA;
  else if (!w.pamiecWBudzecie) d.powod = Powod::PAMIEC;
  else if (w.najwiekszyBlokPsram < PSRAM_BLOK_MIN_B) d.powod = Powod::PSRAM;  // 0 też; BLOK_NIEZNANY nie
  else if (w.najwiekszyBlokWewn < WEWN_BLOK_MIN_B) d.powod = Powod::WEWN_BLOK;  // 0 też; BLOK_NIEZNANY nie
  else if (w.sekundOdStartu >= RESTART_NAJPOZNIEJ_PO_S || w.przebiegow >= PRZEBIEGOW_NAJWIECEJ ||
           (w.sekundOdStartu >= PLANOWY_RESTART_PO_S && w.minutaDoby >= OKNO_RESTARTU_OD_MIN &&
            w.minutaDoby < OKNO_RESTARTU_DO_MIN))
    d.powod = Powod::PLANOWY_RESTART;
  else {
    d.wybor = Wybor::CZEKANIE;
    d.powod = Powod::CZEKANIE;
  }
  return d;
}

const char* opis(Powod p) {
  switch (p) {
    case Powod::CZEKANIE: return "czekanie";
    case Powod::BEZ_OBSLUGI: return "bez obsługi czekania";
    case Powod::NIE_KABEL: return "kabel odłączony albo stan zasilania nieznany";
    case Powod::WYLACZONE: return "wyłączone w konfiguracji (kabel_bez_snu=nie)";
    case Powod::USTAWIENIE_NIEZNANE: return "konfiguracja nieczytelna, kabel_bez_snu nieznane";
    case Powod::NVS_NOWSZY: return "NVS z nowszej wersji, tylko do odczytu";
    case Powod::PO_BLEDZIE: return "restart po błędzie, do końca doby głęboki sen";
    case Powod::AWARYJNY: return "tryb awaryjny";
    case Powod::BEZ_BUDZIKA: return "plan bez budzika";
    case Powod::PAMIEC: return "pamięć poza budżetem";
    case Powod::PSRAM: return "mały wolny blok PSRAM";
    case Powod::WEWN_BLOK: return "mały wolny blok pamięci wewnętrznej";
    case Powod::PLANOWY_RESTART: return "planowy restart";
  }
  return "?";
}

bool przerwacCzekanie(Zasilanie teraz) { return teraz != Zasilanie::ZEWNETRZNE; }

PrzyciskKonczacy przyciskKonczacy(uint8_t maska, const KlawiszCzekania klawisze[4], const NacisniecieCzekania* kolejka,
                                  int ile) {
  PrzyciskKonczacy w;
  int h = 0;
  for (int i = 1; i <= 3 && !h; i++)
    if (klawisze[i].wcisniety && !klawisze[i].pomin && (maska & (1u << i))) h = i;
  int q = -1;
  for (int j = 0; j < ile && q < 0; j++)
    if (kolejka[j].przycisk >= 1 && kolejka[j].przycisk <= 3 && (maska & (1u << kolejka[j].przycisk))) q = j;
  bool zKolejki = q >= 0;
  if (zKolejki && h) {
    const NacisniecieCzekania& n = kolejka[q];
    int64_t odQ = n.puszczonyUs ? n.puszczonyUs - (int64_t)n.trwaloMs * 1000 : 0;
    // trzymany wciśnięty wcześniej niż naciśnięcie z kolejki: najpierw on (kolejka zostaje na następny przebieg)
    if (klawisze[h].odUs != 0 && odQ != 0 && klawisze[h].odUs < odQ) zKolejki = false;
  }
  if (zKolejki) {
    const NacisniecieCzekania& n = kolejka[q];
    w.bit = (uint8_t)(1u << n.przycisk);
    w.zdjacZKolejki = q + 1;
    w.zKolejki = true;
    w.trwaloMs = n.trwaloMs;
    w.odUs = n.puszczonyUs ? n.puszczonyUs - (int64_t)n.trwaloMs * 1000 : 0;
    return w;
  }
  if (h) {
    w.bit = (uint8_t)(1u << h);
    w.klawisz = h;
    w.odUs = klawisze[h].odUs;
    // naciśnięcia spoza maski przed pierwszym w masce przepadają (po głębokim śnie kolejka też byłaby pusta);
    // naciśnięcie w masce wciśnięte później zostaje w kolejce na ten przebieg, po budzącym
    w.zdjacZKolejki = q < 0 ? ile : q;
    return w;
  }
  w.zdjacZKolejki = ile;  // nic w masce: wszystko z kolejki spoza maski przepada (jak dotąd)
  return w;
}

const char* const NVS_KLUCZ_AKTYWNE = "cz_akt";
const char* const NVS_KLUCZ_BLOKADA = "cz_blok";

bool startPoBledzie(PrzyczynaStartu p) {
  return p == PrzyczynaStartu::PANIKA || p == PrzyczynaStartu::TASK_WDT || p == PrzyczynaStartu::INT_WDT ||
         p == PrzyczynaStartu::WDT;
}

Blokada blokada(const std::string& zapisana, const std::string& dzis) {
  if (zapisana.empty()) return Blokada::BRAK;
  if (zapisana == "?") return dzis.empty() ? Blokada::TRWA : Blokada::USTAL_DZIS;
  if (dzis.empty()) return Blokada::TRWA;  // zegar nieznany: zostaje
  long z = nrDnia(zapisana), t = nrDnia(dzis);
  if (z < 0 || (t >= 0 && z > t + 1)) return Blokada::PRZYSZLA;  // zła data albo dalej niż 1 doba w przyszłości
  if (zapisana >= dzis) return Blokada::TRWA;  // ta doba (albo następna: zegar korygowany o godziny)
  return Blokada::KONIEC;
}

}  // namespace czekanie
