// SPDX-License-Identifier: GPL-3.0-or-later
// Decyzje jednego wybudzenia (src/wybudzenie.h). Opis każdej funkcji i wymagania: w nagłówku.
// Testy: test/testy_wybudzenie.cpp.
#include "wybudzenie.h"
#include <cstdio>
#include "cykl.h"
#include "dzien.h"

bool zegarZnany(uint32_t teraz) { return teraz > CZAS_ZNANY_OD; }

// Każde wybudzenie poza przyciskiem: odczyt i poranna decyzja o pytaniu.
Pobranie pobranieNaStarcie(PowodWybudzenia p) {
  Pobranie w;
  if (p != PowodWybudzenia::PRZYCISK) {
    w.pobierz = true;
    w.poranekCzynnikow = true;
  }
  return w;
}

// Ponowienie pobrania (↻) albo wejście na ekran testu: odczyt przerywalny przyciskiem. Przy ↻ na ekranie testu
// zaraz po wybudzeniu odczyt jest jeden.
Pobranie pobraniePoNacisnieciu(const Przejscie& pr, Ekran przed, bool przyciskBudzacy) {
  Pobranie w;
  if (pr.akcja == Akcja::PONOW_POBRANIE) {
    w.pobierz = w.przerywalne = w.poranekCzynnikow = true;
  } else if (pr.ekran == Ekran::TEST && pr.akcja != Akcja::SIEC_PANELU_ANULUJ &&  // powrót z U-02 bez pobrania
             (przyciskBudzacy || przed != Ekran::TEST)) {
    w.pobierz = w.przerywalne = true;
  }
  return w;
}

// START → ekran dnia; po upływie czasu powrót (R-PRZ-02); ekran testu po zegarze → ekran dnia.
Ekran ekranPoStarcie(PowodWybudzenia p, Ekran zRtc, bool zegar, long sekundOdNacisniecia, int minuta) {
  if (p == PowodWybudzenia::START) return Ekran::DZIEN;
  if (p == PowodWybudzenia::PRZYCISK) return zRtc;
  Ekran e = zRtc;
  // R-MAS-14: upływ 30 s na `Nowy model maski zapisany.` wraca do „Sprzęt”
  // R-KONF-05: U-01 po 5 min wraca do K2
  if (zegar && sekundDoPowrotu(e, sekundOdNacisniecia, minuta) == 0)
    e = e == Ekran::SPRZET_ZAPISANO ? Ekran::SPRZET : e == Ekran::USTAWIENIA ? Ekran::K2
        : e == Ekran::AKTUALIZACJA ? Ekran::USTAWIENIA : Ekran::DZIEN;  // R-KONF-44: U-07 po 5 min wraca do U-01
  if (e == Ekran::TEST) e = Ekran::DZIEN;
  if (e == Ekran::FORMULARZ) e = Ekran::USTAWIENIA;  // serwer formularza nie przeżywa snu (R-KONF-15)
  if (e == Ekran::SIEC_PANELU) e = Ekran::USTAWIENIA;  // sieć panelu nie przeżywa snu
  return e;
}

// Zegar poza terminem przerysowania: według decyzji nocy; przycisk bez zmiany w slocie nocnym: bez rysowania (R-KON-37).
bool rysowacPoWybudzeniu(PowodWybudzenia p, bool zmiana, bool zegar, int minuta, bool rysujPoZegarze) {
  if (p == PowodWybudzenia::ZEGAR && zegar && !przerysowacPoZegarze(minuta)) return rysujPoZegarze;
  if (p == PowodWybudzenia::PRZYCISK && !zmiana && zegar && slotNocny(minuta) && !rysujPoZegarze) return false;
  return true;
}

// Pętla czuwania: rysuje przy zmianie, bez zegara albo poza slotem nocnym.
bool rysowacPoNacisnieciu(bool zmiana, bool zegar, int minuta) { return zmiana || !zegar || !slotNocny(minuta); }

// Okno serwisowe tylko na ekranie testu.
bool otworzycOknoSerwisowe(Ekran e) { return e == Ekran::TEST; }

// R-KON-35: ekran dnia, znany wieczór, noc jeszcze bez decyzji i ekran S5.
bool ekranDniaTrwa(Ekran e, const std::string& wieczor, const StanNocy& stan, bool s5) {
  return e == Ekran::DZIEN && !wieczor.empty() && stan.wieczor != wieczor && s5;
}

// Pętla czuwania trwa dłużej, gdy okno serwisowe jest otwarte.
uint32_t czasCzuwaniaMs(bool oknoSerwisowe) { return oknoSerwisowe ? OKNO_SERWISOWE_MS : CZUWANIE_MS; }

// Różnica czasów jak na panelu (32-bitowy long): przez int32_t, więc chwila naciśnięcia
// z przyszłości (zmiana zegara) daje ujemną liczbę sekund także na komputerze.
PlanSnu planSnu(bool zegar, uint32_t teraz, Ekran e, uint32_t ekranOd, int minuta, uint8_t wcisniete) {
  PlanSnu p;
  uint32_t s = OKRES_SNU_S;
  if (zegar) s = OKRES_SNU_S - (teraz % OKRES_SNU_S) + ZAPAS_POLGODZINY_S;
  long powrot = sekundDoPowrotu(e, (long)(int32_t)(teraz - ekranOd), minuta);
  if (zegar && powrot >= 0 && (uint32_t)powrot + ZAPAS_POWROTU_S < s) s = (uint32_t)powrot + ZAPAS_POWROTU_S;
  long kc = sekundDoKoncaCofniecia(e, (long)(int32_t)(teraz - ekranOd), minuta);  // (R-PRZ-19)
  if (zegar && kc >= 0 && (uint32_t)kc + ZAPAS_POWROTU_S < s) s = (uint32_t)kc + ZAPAS_POWROTU_S;
  p.budzZegarem = true;
  p.sekund = s;
  p.przyciski = (uint8_t)(WSZYSTKIE_PRZYCISKI & ~wcisniete);
  return p;
}

// Kolejność sprawdzania bitów: lewy, środkowy, prawy (T-WYM-21).
Przycisk przyciskBudzacy(uint8_t maskaExt1) {
  if (maskaExt1 & BIT_LEWY) return Przycisk::LEWY;
  if (maskaExt1 & BIT_SRODEK) return Przycisk::SRODEK;
  return Przycisk::PRAWY;
}

// Puszczony = krótko; wciśnięty dłużej niż próg = długo; inaczej przytrzymanie trwa (R-PRZ-08).
Przytrzymanie przytrzymanie(uint32_t odMs, uint32_t terazMs, bool wcisniety) {
  if (!wcisniety) return Przytrzymanie::KROTKO;
  return terazMs - odMs >= PROG_PRZYTRZYMANIA_MS ? Przytrzymanie::DLUGO : Przytrzymanie::TRWA;
}

// Kolejność lewy, środkowy, prawy; puszczony przycisk przestaje być zacięty (T-WYM-29).
Przycisk wcisnietyPrzycisk(uint8_t wcisniete, uint8_t& zaciete) {
  for (Przycisk p : {Przycisk::LEWY, Przycisk::SRODEK, Przycisk::PRAWY}) {
    uint8_t bit = (uint8_t)(1u << (int)p);
    if (!(wcisniete & bit)) {
      zaciete = (uint8_t)(zaciete & ~bit);
      continue;
    }
    if (!(zaciete & bit)) return p;
  }
  return Przycisk::BRAK;
}

// Przycisk nadal wciśnięty po czekaniu zostaje oznaczony jako zacięty.
uint8_t zacietePoCzekaniu(Przycisk nadalWcisniety, uint8_t zaciete) {
  if (nadalWcisniety == Przycisk::BRAK) return zaciete;
  return (uint8_t)(zaciete | (1u << (int)nadalWcisniety));
}

// Decyzja po `/api/status`: brak odpowiedzi, E-03, blokada raportów, przetwarzanie albo dalej.
PoStatusie poStatusie(int kod, const std::string& tresc, StatusAB& st) {
  if (kod != 200) return PoStatusie::BRAK_ODPOWIEDZI;
  if (!parsujStatus(tresc, st)) return PoStatusie::E03;
  WersjaAB wer = parsujWersje(st.wersja);
  if (wer.ok() && !wersjaObslugiwana(wer)) return PoStatusie::E03;
  if (raportyZablokowane(st)) return PoStatusie::ZABLOKOWANE;
  if (modulPrzetwarza(st)) return PoStatusie::PRZETWARZA;
  return PoStatusie::DALEJ;
}

// Przy blokadzie raportów: noc „w budowie” tylko przed 12:00 i gdy nocy z dziś jeszcze nie ma.
bool wBudowiePrzyBlokadzie(bool dataZnana, int minuta, bool nocJest) { return dataZnana && minuta < 12 * 60 && !nocJest; }
// Przy przetwarzaniu w module: noc „w budowie”, gdy nocy z dziś jeszcze nie ma.
bool wBudowiePrzyPrzetwarzaniu(bool dataZnana, bool nocJest) { return dataZnana && !nocJest; }

// 200 z ciałem nieczytelnym albo urwanym to połączenie urwane, inne kody bez zmian.
int kodPoNieudanej(int kod) { return kod == 200 ? AB_KOD_URWANE : kod; }

// Koniec, gdy następne odpytanie nie zmieści się w limicie raportu albo w budżecie wybudzenia.
bool koniecOdpytywania(uint32_t uplynelo, uint32_t limitMs, long zostaloMs) {
  return uplynelo + AB_ODPYTYWANIE_MS >= limitMs || zostaloMs <= (long)AB_ODPYTYWANIE_MS;
}

// Zaległe doby tylko po zegarze albo starcie, poza ekranem testu, po udanym odczycie i poza slotem nocnym.
bool dociagacZalegle(const WarunkiZaleglych& w) {
  if (w.powod == PowodWybudzenia::PRZYCISK || w.ekran == Ekran::TEST) return false;
  if (!w.odczytUdany || !w.listaOk || w.zablokowane || !w.plikiOk || !w.wifi) return false;
  return w.dataZnana && !slotNocny(w.minuta);
}

// Bez doby z dziś, najwyżej ZALEGLE_NA_WYBUDZENIE.
std::vector<long> planZaleglych(const std::vector<long>& plan, const std::string& dzis) {
  std::vector<long> w;
  for (long d : plan) {
    if ((int)w.size() >= ZALEGLE_NA_WYBUDZENIE) break;
    if (dataPoranka(d) != dzis) w.push_back(d);
  }
  return w;
}

// Za mało czasu na raport kolejnej doby kończy dociąganie.
bool zaczacRaportZaleglej(long zostaloMs) { return zostaloMs >= (long)MIN_NA_RAPORT_ZALEGLY_MS; }

// Udany odczyt zeruje początek błędu; pierwszy nieudany przy znanym zegarze go ustawia.
uint32_t bladOdPoOdczycie(bool udany, uint32_t bladOd, bool zegar, uint32_t teraz) {
  if (udany) return 0;
  if (!bladOd && zegar) return teraz;
  return bladOd;
}

// S5 kartą z budzikiem albo ekranem danych (U6) i stan terapii z tego odczytu.
bool kartaBraku(int httpKod) { return httpKod != KOD_WIFI && httpKod != KOD_E03 && httpKod != 401; }
bool terapiaTrwa(bool odczytUdany, int rop) { return odczytUdany && rop == 1; }

// Ziarno z NVS, a bez niego losowe i nieparzyste (nigdy 0).
uint32_t ziarnoPanelu(uint32_t zNvs, uint32_t losowa) { return zNvs ? zNvs : (losowa | 1u); }

bool pamiecWBudzecie(const PamiecWybudzenia& p) { return p.minSterta >= PROG_STERTY_B && p.zapasStosu >= PROG_STOSU_B; }

std::string wierszPamieci(const PamiecWybudzenia& p) {
  char b[96];
  snprintf(b, sizeof b, "pamiec sterta_min=%lu stos_zapas=%lu%s", (unsigned long)p.minSterta,
           (unsigned long)p.zapasStosu, pamiecWBudzecie(p) ? "" : " poza_budzetem");
  return b;
}

// ---------- tryb awaryjny ----------

PlanSnu planSnuAwaryjny(uint8_t wcisniete) {
  PlanSnu p;
  p.przyciski = (uint8_t)(WSZYSTKIE_PRZYCISKI & ~wcisniete);
  p.budzZegarem = p.przyciski == 0;  // wszystkie zacięte: bez zegara panel nie obudziłby się do zaniku zasilania
  p.sekund = p.budzZegarem ? OKRES_SNU_S : 0;
  return p;
}

// Przy nieczytelnym statusie parsujStatus() czyści strukturę, więc wersja jest pusta.
bool statusCzytelny(PoStatusie ps, const StatusAB& st) { return ps != PoStatusie::E03 || !st.wersja.empty(); }

// Kontekst mapy przycisków dla ekranu na panelu.
KontekstNaw kontekstNawigacji(Ekran e, const DecyzjaNocy& dn, bool s5, bool s10, bool zaproszenie, bool ostatniaNoc) {
  KontekstNaw k;
  k.nocny = !dn.nowa && dn.nocny;  // S9 na ekranie (narysowany tej nocy): ▶ → K1, ◀ i ↻ nic
  k.s5 = e == Ekran::DZIEN && !k.nocny && (s5 || s10);  // ↻ na S5 i S10 ponawia pobranie
  k.zaproszenie = zaproszenie;
  k.ostatniaNoc = ostatniaNoc;
  return k;
}

// Linia uwagi ekranu dnia: zaproszenie albo E-08.
LiniaDnia liniaDnia(bool zaproszenieAktywne, const std::string& typ, const Otoczenie& o, uint32_t teraz,
                    const std::string& dzis, StanE08& st, bool co2) {
  LiniaDnia l;
  l.zaproszenie = zaproszenieAktywne && typ != "S5";  // R-CZY-17 fail-path: na S5 linia nie powstaje
  if (!l.zaproszenie && typ != "S5" && co2) {
    bool swiezaOdp = o.stan == (int8_t)StanOto::OK && swiezy(o.pobrano, teraz, OTO_SWIEZOSC_OSTRZEZEN_S);
    bool zm = false;
    l.e08 = pokazE08(co2MilczyTeraz(o, teraz), swiezaOdp && !o.co2Milczy, o.co2MilczyOd, dzis, st, zm);
    l.zapiszE08 = zm;
  }
  return l;
}

// Ten sam wybór co przy pierwszym wyświetleniu dnia albo nowy wybór.
PlanWyboru planWyboru(const WpisHumoru* juz, const std::string& typ, bool jestLinia) {
  PlanWyboru p;
  if (juz && typ != "S5") {
    p.zWpisu = true;
    p.wybor.tryb = jestLinia ? 'D' : juz->tryb;  // R-HUM-17: linia pojawiła się później w ciągu dnia
    p.wybor.scena = juz->scena;
    p.wybor.zdanie = juz->zdanie;
    return p;
  }
  p.dopisz = !juz;
  return p;
}

// Zaproszenie oznaczone jako pokazane przy pierwszej linii (R-PIE-08).
bool oznaczycZaproszenie(bool linia, const WpisCzynnikow* zap) { return linia && zap && !zap->pokazano; }

// S10 (R-EKR-57).
bool kartaS10(Humor h, const std::string& zdanie, bool scenaB05, bool miesci) {
  return h != Humor::WYLACZONY && !zdanie.empty() && scenaB05 && miesci;
}
