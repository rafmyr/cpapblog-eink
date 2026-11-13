// SPDX-License-Identifier: GPL-3.0-or-later
// Plan wybudzenia (src/plan.h, T-KOMP-24): terminy doby z R-NF-02 jako minuty siatki, powody bieżącego wybudzenia,
// Wi-Fi tylko z powodem sieciowym, czuwanie tylko po przycisku (krok 2 z 13.1).
// Testy: test/testy_plan_wybudzen.cpp, test/testy_dyrygenta.cpp (dyrygent na planie); slot dzienny:
// test/testy_slot_dzienny_w_planie.cpp.
#include "plan.h"
#include <algorithm>
#include "sypialnia.h"
#include "wspolne.h"

namespace {

const int MINUT_DOBY = 24 * 60;
const int SEKUND_DOBY = 24 * 3600;

// Ponowienia co 3 h od drugiej nocy bez danych (R-DANE-18): 14:00, 17:00, 20:00, 23:00.
bool terminRzadki(int m) { return m > PLAN_OKNO_DO && m < PLAN_NOC_OD && (m - PLAN_OKNO_DO) % PLAN_CO_ILE_RZADKO_MIN == 0; }

// Powody z pełnym pobraniem (AirBridge: status, lista i raporty, zaległe doby; źródło otoczenia).
const Powody PELNE = POWOD_OKNO_PORANNE | POWOD_SPRAWDZENIE_11 | POWOD_PONOWIENIE | POWOD_POGODA |
                       POWOD_POCZATEK_NOCY | POWOD_CZAS | POWOD_OKNO_GESTE | POWOD_POWTORKA |
                       POWOD_OKNO_ZASILANIE;
// Powody slotu dziennego z Wi-Fi tylko na `/api/status` i źródło otoczenia (R-KON-42) albo prognozę (R-KON-52).
const Powody SLOT_SIECIOWE = POWOD_SLOT_DZIENNY_START | POWOD_SLOT_DZIENNY_GODZINA | POWOD_PROGNOZA_GODZINOWA;
const Powody SIECIOWE = PELNE | SLOT_SIECIOWE | POWOD_SLOT_WIECZORNY;  // wieczór tylko status i otoczenie
// Wszystkie bity slotu dziennego (powodyTeraz(): po końcu slotu bez nich).
const Powody SLOT_WSZYSTKIE = SLOT_SIECIOWE | POWOD_SLOT_DZIENNY_BEZ_SIECI;

bool sekundaWDobie(int s) { return s >= 0 && s < SEKUND_DOBY; }
// koniec slotu dziennego w sekundach doby (koniecSlotuDziennego, początek okna slotu wieczornego)
long sekundaKoncaSlotu(const WejsciePlanu& w) { return (long)koniecSlotuDziennego(w.wieczorOd) * 60; }

// Start slotu, który już był (slotDziennyOd), albo -1. Slot bez nocy (E-01, start najwcześniej o 11:00) sprzed importu
// nocy przestaje się liczyć z chwilą importu: R-KON-41 daje start 60 min po imporcie. Start
// przed importem i przed 11:00 to slot z importu, a import późniejszy to ponowny zapis tej samej nocy (bez drugiego
// startu); dyrygent i tak trzyma chwilę pierwszego importu doby (StanRtc::importNocyEp).
int slotJuzOd(const WejsciePlanu& w) {
  if (!sekundaWDobie(w.slotDziennyOd)) return -1;
  if (sekundaWDobie(w.importNocy) && w.slotDziennyOd < w.importNocy && w.slotDziennyOd >= PLAN_OKNO_DO * 60) return -1;
  return w.slotDziennyOd;
}

// Slot dzienny wystartował albo wystartuje naciśnięciem (R-KON-41: naciśnięcie na ekranie dnia po imporcie): wtedy
// bez terminu startu, a prognoza godzinowa przy pierwszej ocenie o pełnej godzinie (R-KON-52, przypadek graniczny).
bool startPrzyciskiem(const WejsciePlanu& w) {
  if (!sekundaWDobie(w.importNocy) || !sekundaWDobie(w.nacisniecieRano) || w.nacisniecieRano < w.importNocy) return false;
  return slotJuzOd(w) < 0 || w.slotDziennyOd == w.nacisniecieRano;
}

// Slot dzienny bez nocy przy E-01 (R-KON-41, przypadek graniczny): start i oceny bez Wi-Fi.
bool slotBezSieci(const WejsciePlanu& w) { return w.e01 && !sekundaWDobie(w.importNocy); }

// Minuta doby terminu startu slotu dziennego (R-KON-41) albo -1 (już wystartował, start naciśnięciem, brak slotu).
int minutaStartuSlotu(const WejsciePlanu& w) {
  if (slotJuzOd(w) >= 0 || startPrzyciskiem(w)) return -1;
  int s = poczatekSlotuDziennego(w);
  if (s < 0) return -1;
  int godzina = (s / 3600 + 1) * 60;  // pierwsza pełna godzina po starcie (minuta doby)
  // start tuż przed pełną godziną idzie razem z oceną o niej (jedno wybudzenie z Wi-Fi)
  if (godzina < koniecSlotuDziennego(w.wieczorOd) && godzina * 60 - s < PLAN_START_Z_OCENA_S) return godzina;
  return s / 60;
}

// Minuta doby, w której przypada pobranie prognozy godzinowej (R-KON-52) albo -1: przy starcie slotu (termin startu),
// przy starcie naciśnięciem w pierwszej ocenie o pełnej godzinie; tylko z miastem i prognozą starszą niż 3 h albo bez.
int minutaPrognozy(const WejsciePlanu& w) {
  if (!w.miasto) return -1;
  int m = minutaStartuSlotu(w);
  if (m < 0 && startPrzyciskiem(w)) {
    int s = poczatekSlotuDziennego(w);
    m = s < 0 ? -1 : (s / 3600 + 1) * 60;
    if (m >= koniecSlotuDziennego(w.wieczorOd)) m = -1;
  }
  if (m < 0) return -1;
  bool stara = !sekundaWDobie(w.prognozaGodzinowa) || m * 60 - w.prognozaGodzinowa > PLAN_PROGNOZA_WAZNA_S;
  return stara ? m : -1;
}

// Powody slotu dziennego chwili siatki `m` (R-KON-41, R-KON-42, R-KON-52).
Powody powodySlotu(const WejsciePlanu& w, int m) {
  int s = poczatekSlotuDziennego(w);
  if (s < 0 || m >= koniecSlotuDziennego(w.wieczorOd)) return 0;
  Powody p = 0;
  bool bezSieci = slotBezSieci(w);
  if (m == minutaStartuSlotu(w)) p |= bezSieci ? POWOD_SLOT_DZIENNY_BEZ_SIECI : POWOD_SLOT_DZIENNY_START;
  if (m % 60 == 0 && m * 60 > s) p |= bezSieci ? POWOD_SLOT_DZIENNY_BEZ_SIECI : POWOD_SLOT_DZIENNY_GODZINA;
  if (m == minutaPrognozy(w)) p |= POWOD_PROGNOZA_GODZINOWA;
  return p;
}

// Czy chwila siatki `m` leży w slocie dziennym (od jego początku do koniecSlotuDziennego()): R-EKR-60 tam milczy.
bool wSlocieDziennym(const WejsciePlanu& w, int m) {
  int s = poczatekSlotuDziennego(w);
  return s >= 0 && m * 60 >= s && m < koniecSlotuDziennego(w.wieczorOd);
}

// Minuta doby przerysowania ekranu dnia (R-EKR-60) albo -1: na panelu ekran dnia narysowany dziś (dzienRysowany),
// termin = minuta rysowania + 2 h, najwcześniej 11:00, przed początkiem okna slotu wieczornego.
int minutaOdswiezenia(const WejsciePlanu& w) {
  if (w.ekran != Ekran::DZIEN || w.dzienRysowany < 0 || w.dzienRysowany >= SEKUND_DOBY) return -1;
  int m = w.dzienRysowany / 60 + PLAN_ODSWIEZENIE_CO_MIN;
  if (m < PLAN_ODSWIEZENIE_OD) m = PLAN_ODSWIEZENIE_OD;
  return m < koniecSlotuDziennego(w.wieczorOd) ? m : -1;  // do początku okna wieczornego (R-EKR-60)
}

// Powody chwili siatki `m` (minuta doby 0…1439) przy stanie doby `w` (bez powrotu: ten nie leży na siatce).
Powody powodyMinuty(const WejsciePlanu& w, int m) {
  Powody p = 0;
  bool co30 = m % PLAN_CO_ILE_MIN == 0;
  if (m == PLAN_OKNO_OD || (m > PLAN_OKNO_OD && m < PLAN_OKNO_DO && co30 && !w.nocKompletna)) p |= POWOD_OKNO_PORANNE;
  // R-DANE-61: gęste okno co 5 min wokół zwykłej pory końca terapii, dopóki noc niekompletna; minuty siatki 30 min
  // zostają z samym POWOD_OKNO_PORANNE
  // bez gęstego okna, gdy ostatnia próba skończyła się brakiem odpowiedzi AirBridge albo Wi-Fi (decyzja właściciela
  // projektu 10.11.2026) albo odrzuconym logowaniem: E-01 w każdej postaci
  if (!w.nocKompletna && !(p & POWOD_OKNO_PORANNE) && m % PLAN_GESTE_CO_MIN == 0 && !w.e01 && wGestymOknie(w, m))
    p |= POWOD_OKNO_GESTE;
  // R-DANE-63, R-DANE-64 (decyzja właściciela projektu 13.11.2026): na zasilaniu zewnętrznym dochodzą minuty co
  // PLAN_ZASILANIE_CO_MIN w oknie porannym i co PLAN_ZASILANIE_GESTE_CO_MIN w gęstym oknie; terminy baterii wyżej
  // zostają ze swoimi powodami. Te same wyłączenia co gęste okno: noc kompletna i E-01 w każdej postaci.
  if (w.naZasilaniu && !w.nocKompletna && !w.e01 && !(p & (POWOD_OKNO_PORANNE | POWOD_OKNO_GESTE)) &&
      m > PLAN_OKNO_OD && m < PLAN_OKNO_DO &&
      m % (wGestymOknie(w, m) ? PLAN_ZASILANIE_GESTE_CO_MIN : PLAN_ZASILANIE_CO_MIN) == 0)
    p |= POWOD_OKNO_ZASILANIE;
  if (m == PLAN_OKNO_DO && !w.sprawdzenie11) p |= POWOD_SPRAWDZENIE_11;
  if (m > PLAN_OKNO_DO && m <= PLAN_NOC_OD && co30 && !w.nocKompletna) {
    bool rzadko = w.e01 && w.nocyBezDanych >= 2 && !w.logowanieOdrzucone;
    if (!rzadko || terminRzadki(m)) p |= POWOD_PONOWIENIE;
  }
  if (m == PLAN_POGODA_WIECZOR && w.zrodloOtoczenia) p |= POWOD_POGODA;
  if (m == PLAN_NOC_OD) p |= POWOD_POCZATEK_NOCY;
  // R-KON-01: ocena warunków wieczornych co 30 min w oknie slotu wieczornego (do 23:30), gdy jest źródło CO2
  // co 30 min od początku okna
  // tylko z włączonymi ekranami CO2 (WejsciePlanu::co2)
  // (R-KON-01): bez CO2 te same minuty bez Wi-Fi (ocena S-K-03…S-K-09 z danych w pamięci)
  if (w.wieczorOd >= 0 && m >= w.wieczorOd && m < PLAN_NOC_OD && (m - w.wieczorOd) % PLAN_CO_ILE_MIN == 0)
    p |= w.zrodloOtoczenia && w.co2 ? POWOD_SLOT_WIECZORNY : POWOD_SLOT_WIECZORNY_BEZ_SIECI;
  if (w.niedziela && m == 17 * 60) p |= POWOD_SLOT_TYGODNIOWY;  // R-KON-08: niedziela 17:00
  if (oknoSypialni(m) && m % PLAN_CZUJNIKI_CO_MIN == 0) p |= POWOD_CZUJNIKI;
  if (w.terminPytania >= 0 && m == w.terminPytania) p |= POWOD_PYTANIE;
  p |= powodySlotu(w, m);  // slot dzienny (R-KON-41, R-KON-42, R-KON-52)
  // R-EKR-60: przerysowanie ekranu dnia 2 h po ostatnim narysowaniu, nie przed 11:00, przed oknem slotu wieczornego;
  // nie w minucie z powodem sieciowym (to wybudzenie i tak rysuje ekran dnia); tylko poza slotem dziennym (w nim
  // ocena R-KON-42, także bez Wi-Fi przy E-01)
  if (!(p & (SIECIOWE | POWOD_SLOT_DZIENNY_BEZ_SIECI)) && m == minutaOdswiezenia(w) && !wSlocieDziennym(w, m))
    p |= POWOD_ODSWIEZENIE;
  // R-POR-04: drugi ekran poranka 30 min po pierwszym narysowaniu Portretu, gdy slot dzienny jeszcze nie wystartował
  if (w.drugiEkranPortretu >= 0 && m == w.drugiEkranPortretu && w.slotDziennyOd < 0) p |= POWOD_ODSWIEZENIE;
  return p;
}

long sekundaTeraz(const WejsciePlanu& w) {
  return w.sekundaDoby < 0 ? 0 : (w.sekundaDoby >= SEKUND_DOBY ? SEKUND_DOBY - 1 : w.sekundaDoby);
}

// Powody chwil siatki w przedziale [od, teraz] (sekundy doby dziś).
Powody powodyPrzedzialu(const WejsciePlanu& w, long od, long teraz) {
  Powody p = 0;
  if (od < 0) od = 0;
  for (long m = (od + 59) / 60; m * 60 <= teraz; m++) p |= powodyMinuty(w, (int)m);
  return p;
}

// Czy obsłużona chwila nadaje się na początek przedziału (znana i wcześniejsza niż teraz).
bool obsluzonaPrzedTeraz(const WejsciePlanu& w, long teraz) {
  return w.obsluzoneZnane && w.obsluzoneDo >= -1 && w.obsluzoneDo < teraz;
}

// Stan doby następnej po północy: nowa doba bez danych (kontrakt nastepnyTermin()).
WejsciePlanu nowaDoba(const WejsciePlanu& w) {
  WejsciePlanu n = w;
  n.nocKompletna = false;
  n.sprawdzenie11 = false;
  n.terminPytania = -1;
  n.dzienRysowany = -1;  // ekran dnia z wczoraj: 05:00 rysuje nowy
  n.koniecTerapii = n.importNocy = n.nacisniecieRano = n.slotDziennyOd = n.prognozaGodzinowa = -1;  // nowa doba
  n.nocBezTerapii = false;
  n.przetwarzaOd = -1;  // przetwarzanie i powtórki R-DANE-62 liczą się w obrębie doby
  n.powtorekDzis = 0;
  n.drugiEkranPortretu = -1;
  if (n.e01) n.nocyBezDanych++;
  return n;
}

}  // namespace

int koniecSlotuDziennego(int wieczorOd) {
  return wieczorOd >= 0 && wieczorOd < PLAN_NOC_OD ? wieczorOd : PLAN_NOC_OD;
}

Termin nastepnyTermin(const WejsciePlanu& w) {
  Termin t;
  if (!w.zegar) {
    t.sekund = OKRES_SNU_S;
    t.powody = POWOD_CZAS;
    return t;
  }
  Powody zalegle = powodyZalegle(w);
  if (zalegle) {
    t.sekund = ZALEGLY_TERMIN_S;
    t.powody = zalegle;
    return t;
  }
  long teraz = sekundaTeraz(w);
  // R-DANE-62: powtórka przy przetwarzaniu, której chwila już minęła (np. w czuwaniu po przycisku): zaraz
  int powtorka = terminPowtorki(w);
  if (powtorka >= 0 && powtorka <= teraz) {
    t.sekund = ZALEGLY_TERMIN_S;
    t.powody = POWOD_POWTORKA;
    return t;
  }
  WejsciePlanu jutro = nowaDoba(w);
  // pierwsza chwila siatki później niż teraz; 05:00 i czujniki są każdej doby, więc dwie doby zawsze wystarczą
  long chwila = -1;
  Powody powody = 0;
  // przeglądu terminy do obsłużonej chwili po teraz (zegar cofnięty przez NTP) już wykonane
  long od = w.obsluzoneZnane && w.obsluzoneDo > teraz && w.obsluzoneDo < SEKUND_DOBY ? (long)w.obsluzoneDo : teraz;
  for (long m = od / 60 + 1; m < 2L * MINUT_DOBY; m++) {
    Powody p = m < MINUT_DOBY ? powodyMinuty(w, (int)m) : powodyMinuty(jutro, (int)(m - MINUT_DOBY));
    if (p) {
      chwila = m * 60;
      powody = p;
      break;
    }
  }
  // R-DANE-62: powtórka 2 min po statusie „przetwarza”, gdy wypada przed najbliższą chwilą siatki (sekunda, nie minuta)
  if (powtorka >= 0 && (chwila < 0 || powtorka < chwila)) {
    chwila = powtorka;
    powody = POWOD_POWTORKA;
  }
  // powrót R-PRZ-02 (w slocie nocnym sekundDoPowrotu() daje -1, R-PRZ-18)
  int minuta = (int)(teraz / 60);
  long powrot = sekundDoPowrotu(w.ekran, w.sekundOdNacisniecia, minuta);
  if (powrot >= 0 && (chwila < 0 || teraz + powrot <= chwila)) {
    t.sekund = (uint32_t)powrot + ZAPAS_POWROTU_S;
    t.powody = (Powody)(POWOD_POWROT | (chwila >= 0 && teraz + powrot == chwila ? powody : 0));
    return t;
  }
  // (R-PRZ-19): w nocy jedno wybudzenie po czasie cofnięcia ekranu potwierdzenia (przerysowanie bez podpisu)
  long kc = sekundDoKoncaCofniecia(w.ekran, w.sekundOdNacisniecia, minuta);
  if (kc >= 0 && (chwila < 0 || teraz + kc <= chwila)) {
    t.sekund = (uint32_t)kc + ZAPAS_POWROTU_S;
    t.powody = POWOD_POWROT;
    return t;
  }
  if (chwila < 0) {  // nie zdarza się (05:00 jest zawsze); ostrożnie jak prototyp
    t.sekund = OKRES_SNU_S;
    return t;
  }
  t.sekund = (uint32_t)(chwila - teraz) + ZAPAS_POLGODZINY_S;
  t.powody = powody;
  return t;
}

Powody powodyTeraz(const WejsciePlanu& w) {
  if (!w.zegar) return POWOD_CZAS;
  long teraz = sekundaTeraz(w);
  // chwile siatki po ostatniej obsłużonej, bez niej w [teraz − TOLERANCJA_TERMINU_S, teraz]; tylko
  // z dziś (terminy sprzed północy wykonało wybudzenie poprzedniej doby albo były to same czujniki)
  long od = obsluzonaPrzedTeraz(w, teraz) ? (long)w.obsluzoneDo + 1 : teraz - (long)TOLERANCJA_TERMINU_S;
  Powody p = powodyPrzedzialu(w, od, teraz);
  // R-KON-41 (panel uśpiony w chwili startu): start slotu dziennego przy najbliższym wybudzeniu, także gdy chwila
  // startu wypadła poza przedziałem; po końcu slotu dziennego bez powodów slotu (ocena i start już bez znaczenia)
  if (teraz >= sekundaKoncaSlotu(w)) {
    p = (Powody)(p & ~SLOT_WSZYSTKIE);
  } else if (minutaStartuSlotu(w) >= 0 && (long)minutaStartuSlotu(w) * 60 <= teraz) {
    p |= slotBezSieci(w) ? POWOD_SLOT_DZIENNY_BEZ_SIECI : POWOD_SLOT_DZIENNY_START;
    int mp = minutaPrognozy(w);
    if (mp >= 0 && (long)mp * 60 <= teraz) p |= POWOD_PROGNOZA_GODZINOWA;
  }
  // R-DANE-62: chwila powtórki minęła (sekunda, nie minuta siatki); gdy w tym wybudzeniu i tak jest termin okna
  // porannego albo gęstego (status będzie odczytany), powtórka nie zużywa licznika doby
  int powtorka = terminPowtorki(w);
  if (powtorka >= 0 && powtorka <= teraz && !(p & (POWOD_OKNO_PORANNE | POWOD_OKNO_GESTE | POWOD_OKNO_ZASILANIE)))
    p |= POWOD_POWTORKA;
  if (sekundDoPowrotu(w.ekran, w.sekundOdNacisniecia, (int)(teraz / 60)) == 0) p |= POWOD_POWROT;
  if (koniecCofnieciaTeraz(w.ekran, w.sekundOdNacisniecia, (int)(teraz / 60))) p |= POWOD_POWROT;
  return p;
}

Powody powodyZalegle(const WejsciePlanu& w) {
  if (!w.zegar) return 0;
  long teraz = sekundaTeraz(w);
  if (!obsluzonaPrzedTeraz(w, teraz)) return 0;
  return (Powody)(powodyPrzedzialu(w, (long)w.obsluzoneDo + 1, teraz) & (SIECIOWE | POWOD_PYTANIE));
}

int poczatekSlotuDziennego(const WejsciePlanu& w) {
  if (!w.zegar || !w.slotDzienny) return -1;
  int s = -1;
  if (slotJuzOd(w) >= 0) {
    s = w.slotDziennyOd;
  } else if (w.importNocy >= 0) {
    if (!sekundaWDobie(w.importNocy)) return -1;
    if (startPrzyciskiem(w)) {
      s = w.nacisniecieRano;
    } else {
      // koniec terapii spoza doby (np. z wczoraj albo zły) nie przesuwa startu: liczy się sam import
      int od = sekundaWDobie(w.koniecTerapii) && w.koniecTerapii > w.importNocy ? w.koniecTerapii : w.importNocy;
      s = od + PLAN_SLOT_DZIENNY_PO_S;
    }
  } else if (!w.nocKompletna && (w.nocBezTerapii || w.e01)) {  // noc bez terapii albo brak nocy (E-10, E-01)
    s = PLAN_OKNO_DO * 60;
  }
  return s >= 0 && s < sekundaKoncaSlotu(w) ? s : -1;
}

bool tylkoStatus(Powody powody) { return sieciowy(powody) && (powody & PELNE) == 0; }

bool przerysowanieDnia(Powody powody) {
  const Powody dozwolone = POWOD_ODSWIEZENIE | SLOT_WSZYSTKIE | POWOD_CZUJNIKI;
  const Powody rysujace = POWOD_ODSWIEZENIE | POWOD_SLOT_DZIENNY_START | POWOD_SLOT_DZIENNY_GODZINA |
                            POWOD_SLOT_DZIENNY_BEZ_SIECI;
  return (powody & rysujace) != 0 && (powody & (Powody)~dozwolone) == 0;
}

bool startSlotuPrzyciskiem(const WejsciePlanu& w, Ekran przed) {
  if (!w.zegar || !w.slotDzienny || przed != Ekran::DZIEN || slotJuzOd(w) >= 0 || !sekundaWDobie(w.importNocy))
    return false;
  long teraz = sekundaTeraz(w);
  return teraz >= w.importNocy && teraz < sekundaKoncaSlotu(w) && !slotNocny((int)(teraz / 60));
}

bool sieciowy(Powody powody) { return (powody & SIECIOWE) != 0; }

bool tylkoRytmZasilania(Powody powody) {
  return (powody & POWOD_OKNO_ZASILANIE) != 0 && (powody & (Powody)~(POWOD_OKNO_ZASILANIE | POWOD_CZUJNIKI)) == 0;
}

Pobranie pobranieZegarem(Powody powody) {
  Pobranie p;
  p.pobierz = sieciowy(powody);
  p.poranekCzynnikow = p.pobierz || (powody & POWOD_PYTANIE) != 0;
  return p;
}

bool wybudzeniePomiarowe(PowodWybudzenia p, Powody powody) {
  return p == PowodWybudzenia::ZEGAR && (powody & (Powody)~POWOD_CZUJNIKI) == 0;
}

uint32_t czuwanieMs(PowodWybudzenia p, bool oknoSerwisowe) {
  if (oknoSerwisowe) return OKNO_SERWISOWE_MS;
  return p == PowodWybudzenia::PRZYCISK ? CZUWANIE_MS : 0;
}

PlanSnu planSnuZTerminu(const Termin& t, uint8_t wcisniete) {
  PlanSnu p;
  p.budzZegarem = true;
  p.sekund = t.sekund;
  p.przyciski = (uint8_t)(WSZYSTKIE_PRZYCISKI & ~wcisniete);
  return p;
}

std::string nazwyPowodow(Powody powody) {
  static const char* const NAZWY[] = {"okno_poranne", "sprawdzenie_11", "ponowienie", "pogoda", "poczatek_nocy",
                                      "czujniki",     "powrot",         "pytanie",    "czas",
                                      "odswiezenie",  "slot_dzienny",   "slot_dzienny_godzina",
                                      "prognoza_godzinowa", "slot_dzienny_bez_sieci", "slot_wieczorny",
                                      "slot_tygodniowy", "slot_wieczorny_bez_sieci", "okno_geste",
                                      "powtorka_przetwarzania", "okno_zasilanie"};
  std::string s;
  for (unsigned i = 0; i < sizeof NAZWY / sizeof NAZWY[0]; i++) {
    if (!(powody & (1u << i))) continue;
    if (!s.empty()) s += '+';
    s += NAZWY[i];
  }
  return s;
}

std::string powodWpisuBaterii(PowodWybudzenia p, Powody powody) {
  switch (p) {
    case PowodWybudzenia::START: return "start";
    case PowodWybudzenia::PRZYCISK: return "przycisk";
    case PowodWybudzenia::ZEGAR: break;
  }
  return "zegar:" + nazwyPowodow(powody);
}

int nocyBezDanych(bool e01, const std::string& dzis, const std::string& dataOdczytu, int minutaOdczytu,
                  const std::string& dataBledu, const std::vector<Noc>* hist) {
  if (!e01) return 0;
  return nocyBezDanychZRzedu(dzis, dataOdczytu, minutaOdczytu, dataBledu, hist);
}

int nocyBezDanych(bool e01, const std::string& dzis, const std::string& dataOdczytu, const std::string& dataBledu) {
  return nocyBezDanych(e01, dzis, dataOdczytu, -1, dataBledu, nullptr);
}

bool nocKompletnaDzis(const std::vector<Noc>& hist, const std::string& dzis, const std::set<long>& pominiete) {
  if (!dataOk(dzis)) return false;
  const Noc* n = nocZDnia(hist, dzis);
  return (n && n->zrodlo == 'A' && !n->czesciowa) || pominiete.count(nrDnia(dzis) - 1) != 0;
}

int terminPytaniaDzis(const WpisCzynnikow* wpis) {
  if (!wpis || wpis->stan != 'C' || wpis->termin < 0 || wpis->termin >= MINUT_DOBY) return -1;
  return wpis->termin;
}

// ---------- szybszy wynik nocy rano (R-DANE-61, R-DANE-62, R-PRZ-22) ----------

bool wGestymOknie(const WejsciePlanu& w, int minuta) {
  if (w.zwyklyKoniec < 0 || w.zwyklyKoniec >= MINUT_DOBY) return false;
  if (minuta <= PLAN_OKNO_OD || minuta >= PLAN_OKNO_DO) return false;  // 05:00 i 11:00 mają własne terminy
  return minuta >= w.zwyklyKoniec - PLAN_GESTE_PRZED_MIN && minuta <= w.zwyklyKoniec + PLAN_GESTE_PO_MIN;
}

int terminPowtorki(const WejsciePlanu& w) {
  if (!w.zegar || w.nocKompletna || w.powtorekDzis >= PLAN_POWTORKI_MAKS) return -1;
  if (sekundaTeraz(w) >= (long)PLAN_OKNO_DO * 60) return -1;  // tylko w oknie porannym, także termin zaległy
  if (!sekundaWDobie(w.przetwarzaOd) || w.przetwarzaOd < PLAN_OKNO_OD * 60) return -1;
  long t = (long)w.przetwarzaOd + (long)PLAN_POWTORKA_S;
  return t < (long)PLAN_OKNO_DO * 60 ? (int)t : -1;
}

int koniecTerapiiNocy(const Noc& n) {
  if (n.czesciowa || n.m18 < 0 || n.m18 >= MINUT_DOBY || n.m01 < 15) return -1;
  return n.m18 + n.m01;  // minuty od 12:00 wieczoru; > 1440 = koniec po 12:00 dnia poranka
}

int zwyklyKoniecTerapii(const std::vector<Noc>& hist, const std::string& dzis) {
  if (!dataOk(dzis)) return -1;
  std::vector<int> k;
  for (size_t i = hist.size(); i-- > 0 && (int)k.size() < PLAN_GESTE_NOCY;) {
    const Noc& n = hist[i];
    if (n.data >= dzis) continue;  // noc z poranka dziś (i późniejsze) się nie liczy
    int v = koniecTerapiiNocy(n);
    if (v >= 0) k.push_back(v);
  }
  if ((int)k.size() < PLAN_GESTE_NOCY_MIN) return -1;
  std::sort(k.begin(), k.end());
  size_t c = k.size();
  int med = c % 2 ? k[c / 2] : (k[c / 2 - 1] + k[c / 2]) / 2;  // minuty od 12:00 wieczoru
  int m = 12 * 60 + med - MINUT_DOBY;                           // minuta doby poranka
  if (m < 0) return -1;                                         // koniec przed północą: nie dotyczy okna porannego
  return m < MINUT_DOBY ? m : -1;                               // koniec po 12:00 następnego dnia: brak
}

bool pobranieRanoPrzyciskiem(const WejsciePlanu& w, long odOstatniego) {
  // E-01 (brak odpowiedzi AirBridge albo Wi-Fi, odrzucone logowanie): bez pobrania; ekran dnia to wtedy S5
  // z `↻ ponów` (R-PRZ-06), a siatka 30 min sprawdza dalej
  if (!w.zegar || w.nocKompletna || w.e01) return false;
  long teraz = sekundaTeraz(w);
  if (teraz < (long)PLAN_OKNO_OD * 60 || teraz >= (long)PLAN_OKNO_DO * 60) return false;
  return odOstatniego < 0 || odOstatniego >= PLAN_POBRANIE_PRZYCISKIEM_CO_S;
}
