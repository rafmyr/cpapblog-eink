// SPDX-License-Identifier: GPL-3.0-or-later
#include "nawigacja.h"

namespace {
Przejscie pokaz(Ekran e) { return {e, Akcja::POKAZ}; }

// ciąg stron kokpitu (5.5.7, R-KOK-19). ▶ = strona po `e`, ◀ = strona przed `e`; „Co znaczą te liczby” tylko
// w profilu publicznym (R-KOK-22), „Otwarte sprawy” tylko przy co najmniej jednej sprawie (R-KOK-20), inaczej pomijane
// w obie strony.
Przejscie sprawyAlbo(Ekran e, const KontekstNaw& k, bool odKonca) {
  if (k.sprawyDostepne) return {Ekran::SPRAWY, odKonca ? Akcja::SPRAWY_OD_KONCA : Akcja::SPRAWY_OTWORZ};
  return pokaz(e);
}
Przejscie stronaPo(Ekran e, const KontekstNaw& k) {
  switch (e) {
    case Ekran::DZIEN: return pokaz(Ekran::K1);
    case Ekran::K1: return pokaz(Ekran::K2);
    case Ekran::K2: return k.profilPubliczny ? pokaz(Ekran::LICZBY) : sprawyAlbo(Ekran::USTAWIENIA, k, false);
    case Ekran::LICZBY: return sprawyAlbo(Ekran::USTAWIENIA, k, false);
    case Ekran::SPRAWY: return pokaz(Ekran::USTAWIENIA);
    case Ekran::USTAWIENIA: return pokaz(Ekran::FORMULARZ);
    case Ekran::FORMULARZ: return pokaz(Ekran::TEST);
    default: return {e, Akcja::NIC};
  }
}

Przejscie stronaPrzed(Ekran e, const KontekstNaw& k) {
  switch (e) {
    case Ekran::K1: return pokaz(Ekran::DZIEN);
    case Ekran::K2: return pokaz(Ekran::K1);
    case Ekran::LICZBY: return pokaz(Ekran::K2);
    case Ekran::SPRAWY: return pokaz(k.profilPubliczny ? Ekran::LICZBY : Ekran::K2);
    case Ekran::USTAWIENIA: return sprawyAlbo(k.profilPubliczny ? Ekran::LICZBY : Ekran::K2, k, true);
    case Ekran::FORMULARZ:
    case Ekran::TEST: return pokaz(Ekran::USTAWIENIA);  // oba to ekrany Ustawień: ◀ wraca do U-01
    default: return {e, Akcja::NIC};
  }
}
}  // namespace

int numerStrony(Ekran e) {
  switch (e) {
    case Ekran::K1: return 1;
    case Ekran::K2: return 2;
    case Ekran::USTAWIENIA: return 3;
    default: return 0;
  }
}

bool stronaKokpitu(Ekran e) { return numerStrony(e) > 0 || e == Ekran::LICZBY || e == Ekran::SPRAWY; }

// R-PRZ-20: także strona Portretu pod ▶ (R-POR-24): krótkie naciśnięcie na niej niczego nie zapisuje, więc szybkie ▶ ▶
// z ekranu problemu dochodzi do K1
bool zKolejkiWolno(Ekran e, bool dlugo) {
  return !dlugo && (e == Ekran::DZIEN || stronaKokpitu(e) || e == Ekran::PORTRET);
}

int przytnijKarte(int karta, int liczba) { return liczba < 1 ? 1 : karta < 1 ? 1 : karta > liczba ? liczba : karta; }

std::string numerStronyTekst(Ekran e) {
  int n = numerStrony(e);
  return n ? std::to_string(n) + " / " + std::to_string(STRON_KOKPITU) : std::string();
}

Przejscie nacisniecie(Ekran e, Przycisk p, bool dlugo, const KontekstNaw& k) {
  const Przejscie nic = {e, Akcja::NIC};
  const Przejscie dom = pokaz(Ekran::DZIEN);  // krótkie ↻ poza ekranem dnia (wyjątki: pytania, siatka)
  if (p == Przycisk::BRAK) return nic;
  switch (e) {
    case Ekran::DZIEN:
      // (R-KONF-07): bez sieci domowej ◀ 2 s uruchamia sieć panelu, także na ekranie nocnym
      if (p == Przycisk::LEWY && dlugo && k.bezKonfiguracji) return {Ekran::SIEC_PANELU, Akcja::SIEC_PANELU_START};
      if (k.nocny) return p == Przycisk::PRAWY && !dlugo ? pokaz(Ekran::K1) : nic;  // S9: bez pobrania i pytania
      if (k.pytanieTN) {  // pytanie tak/nie (5.12.8, mapa 5.6.2): ◀ nie, ▶ tak, ↻ pomiń; przytrzymania nic
        if (dlugo) return nic;
        if (p == Przycisk::SRODEK) return {e, Akcja::ODP_POMIN};  // pominięcie nie daje „Zapisano” (5.6.2)
        return {Ekran::ZAPISANO, p == Przycisk::LEWY ? Akcja::ODP_NIE : Akcja::ODP_TAK};
      }
      if (k.wzorzec) {  // ekran lekarski L (A, B) i problem L-09, S3 (5.6.2): ▶ K1, ▶ 2 s raport (problem: nic), ↻ 2 s „wiem”
        // ▶ = K1 jak na każdym ekranie dnia, raport tylko przytrzymaniem (jak w K2);
        // krótkie ↻ na L i S3 ma widoczną reakcję (ekran dnia rysowany od nowa), nie „nic”
        if (p == Przycisk::SRODEK) return dlugo ? Przejscie{Ekran::PRZENIESIONE, Akcja::WIEM} : pokaz(Ekran::DZIEN);
        if (p == Przycisk::LEWY) return dlugo && !k.problem ? pokaz(Ekran::OMOWIONE) : nic;  // potwierdzenie zapisu
        if (dlugo) return k.problem ? nic : pokaz(Ekran::RAPORT);
        return pokaz(Ekran::K1);
      }
      if (p == Przycisk::SRODEK) {  // ↻ dom: tu nic, poza ponowieniem pobrania na S5 i S10 (R-PRZ-06)
        // S5 baterii (E-05 krytyczne): nowy pomiar i nowy wybór ekranu dnia zamiast pobrania
        if (!dlugo && k.s5Bateria) return {e, Akcja::POMIAR_BATERII};
        if (!dlugo) return k.s5 ? Przejscie{e, Akcja::PONOW_POBRANIE} : nic;
        return k.zaproszenie ? Przejscie{Ekran::PYTANIE, Akcja::OTWORZ_PYTANIE} : nic;  // odpowiedź = zapis (R-CZY-17)
      }
      // R-MAS-01, R-MAS-18: ◀ 2 s na S-K-31 i S-K-35 zapisuje przez ekran z cofnięciem 30 s
      if (p == Przycisk::LEWY) return dlugo && k.sk31 ? Przejscie{Ekran::SPRZET_ZAPISANO, Akcja::SPRZET_WYMIENIONE} : nic;
      return dlugo ? nic : stronaPo(e, k);
    case Ekran::K1:
      if (p == Przycisk::SRODEK)  // ↻ 2 s: odpowiedź „Co było inaczej?” dla ostatniej nocy (R-CZY-08)
        return dlugo ? (k.ostatniaNoc ? Przejscie{Ekran::PYTANIE, Akcja::OTWORZ_PYTANIE} : nic) : dom;
      if (dlugo) return nic;
      return p == Przycisk::LEWY ? stronaPrzed(e, k) : stronaPo(e, k);
    case Ekran::K2:
      if (p == Przycisk::SRODEK) return dlugo ? nic : dom;
      if (p == Przycisk::PRAWY && dlugo) return pokaz(Ekran::RAPORT);  // raport ogólny (R-LEK-02): usługa z Wi-Fi
      if (dlugo) return nic;
      return p == Przycisk::LEWY ? stronaPrzed(e, k) : stronaPo(e, k);
    case Ekran::SPRAWY:  // R-EKR-47, R-KOK-20: ▶ następna karta, za ostatnią U-01; ◀ poprzednia, przed pierwszą K2
      if (dlugo) return nic;
      if (p == Przycisk::SRODEK) return dom;
      if (p == Przycisk::LEWY) return k.sprawa <= 1 || k.spraw == 0 ? stronaPrzed(e, k) : Przejscie{e, Akcja::SPRAWY_WSTECZ};
      return k.sprawa < k.spraw ? Przejscie{e, Akcja::SPRAWY_DALEJ} : stronaPo(e, k);
    case Ekran::LICZBY:  // „Co znaczą te liczby” (R-KOK-22): ◀ K2, ▶ „Otwarte sprawy” albo U-01
      if (dlugo) return nic;
      if (p == Przycisk::SRODEK) return dom;
      return p == Przycisk::LEWY ? stronaPrzed(e, k) : stronaPo(e, k);
    case Ekran::NORMA:   // dawne strony kokpitu (ADR-0041): poza ciągiem stron, krótkie naciśnięcie = ekran dnia
    case Ekran::SPRZET:
      return dlugo ? nic : dom;
    case Ekran::SPRZET_ZAPISANO:  // R-MAS-01, R-MAS-18: ◀ 2 s cofa, reszta: ekran dnia, zapis zostaje
      if (p == Przycisk::LEWY && dlugo && k.cofniecie) return {Ekran::DZIEN, Akcja::SPRZET_COFNIJ};
      return dom;
    case Ekran::PYTANIE:  // jedyny wyjątek od reguły (5.6.1): ◀ ▶ kursor, ↻ zaznacz, ↻ 2 s zapisz, ◀ 2 s pomiń
      // ↻ 2 s zapisuje dopiero, gdy w siatce coś zmieniono albo wybrano; bez ruchu to „ekran dnia” bez zapisu
      if (p == Przycisk::SRODEK)
        return dlugo ? (k.pytanieRuszone ? Przejscie{Ekran::DZIEN, Akcja::PYT_ZAPISZ} : dom) : Przejscie{e, Akcja::PYT_ZAZNACZ};
      if (p == Przycisk::LEWY) return dlugo ? Przejscie{Ekran::DZIEN, Akcja::PYT_POMIN} : Przejscie{e, Akcja::PYT_WSTECZ};
      return dlugo ? nic : Przejscie{e, Akcja::PYT_DALEJ};
    case Ekran::OMOWIONE:  // potwierdzenie „Omówione z lekarzem?”: ▶ tak, ◀ nie, ↻ dom (anuluj); przytrzymania nic
      if (dlugo) return nic;
      if (p == Przycisk::PRAWY) return k.omowienieZablokowane ? nic : Przejscie{Ekran::DZIEN, Akcja::OMOWIONE_TAK};
      return dom;
    case Ekran::PRZENIESIONE:  // R-PRZ-16, R-PRZ-17: ◀ 2 s cofa „wiem”, każdy inny przycisk: ekran dnia, „wiem” zostaje
      if (p == Przycisk::LEWY && dlugo && k.cofniecie) return {Ekran::DZIEN, Akcja::COFNIJ_WIEM};
      return dom;
    case Ekran::ZAPISANO:  // R-PRZ-11, R-PRZ-12: ◀ 2 s cofa zapis (pytanie wraca), reszta: ekran dnia
      if (p == Przycisk::LEWY && dlugo && k.cofniecie) return {Ekran::DZIEN, Akcja::COFNIJ_ODP};
      return dom;
    case Ekran::RAPORT:  // „Raport dla lekarza”: ◀ tam, skąd otwarto, ▶ dalej od tego ekranu, ↻ dom, ↻ 2 s starsze
      if (p == Przycisk::SRODEK) return dlugo ? Przejscie{e, Akcja::RAPORT_STARSZE} : dom;  // R-LEK-33
      if (dlugo) return nic;
      return p == Przycisk::LEWY ? pokaz(k.zrodlo) : stronaPo(k.zrodlo, k);
    case Ekran::USTAWIENIA:  // U-01 (3 / 3): ◀ sprawy, liczby albo K2, ◀ 2 s sieć panelu (U-02), ▶ nic, ▶ 2 s U-03, ↻ dom
      if (p == Przycisk::LEWY)  // ◀ 2 s = sieć panelu (U-02)
        return dlugo ? Przejscie{Ekran::SIEC_PANELU, Akcja::SIEC_PANELU_START} : stronaPrzed(e, k);
      // ↻ 2 s: U-07 ze sprawdzeniem wersji (R-KONF-43); Wi-Fi tylko przytrzymaniem (5.6.1 pkt 4)
      if (p == Przycisk::SRODEK) return dlugo ? Przejscie{Ekran::AKTUALIZACJA, Akcja::AKT_SPRAWDZ} : dom;
      return dlugo ? stronaPo(e, k) : nic;  // Wi-Fi i serwer formularza tylko przytrzymaniem, nie przeglądaniem
    case Ekran::FORMULARZ:  // U-03: ◀ U-01, ▶ nic, ▶ 2 s U-05, ↻ dom
      if (p == Przycisk::SRODEK) return dlugo ? nic : dom;
      if (p == Przycisk::LEWY) return dlugo ? nic : stronaPrzed(e, k);
      return dlugo ? stronaPo(e, k) : nic;  // okno serwisowe (Wi-Fi 10 min) tylko przytrzymaniem
    case Ekran::TEST:  // U-05: ◀ U-01, ◀ 2 s sieć panelu, ▶ odśwież, ↻ dom; ↻ 2 s pakiet z karty (R-NF-30): brak
      if (p == Przycisk::LEWY) return dlugo ? Przejscie{Ekran::SIEC_PANELU, Akcja::SIEC_PANELU_START} : stronaPrzed(e, k);
      if (dlugo) return nic;
      return p == Przycisk::SRODEK ? dom : Przejscie{e, Akcja::PONOW_POBRANIE};
    case Ekran::PORTRET:  // Portret pod ▶ (R-POR-24): przejścia w dyrygencie (przejsciePortretu()), tu nic
      return nic;
    case Ekran::AKTUALIZACJA:  // U-07: ◀ U-01 („nie teraz”), ▶ tak / spróbuj ponownie, ↻ dom; przytrzymania nic
      if (dlugo) return nic;
      if (p == Przycisk::SRODEK) return dom;
      if (p == Przycisk::LEWY) return pokaz(Ekran::USTAWIENIA);
      if (k.aktualizacja == 1) return {e, Akcja::AKT_INSTALUJ};
      if (k.aktualizacja == 2) return {e, Akcja::AKT_PONOW};
      return nic;
    case Ekran::SIEC_PANELU:  // U-02: ◀ (także 2 s) anuluj, ↻ dom (też anuluje), ↻ 2 s nowe hasło i kod, ▶ nic
      if (p == Przycisk::LEWY) return {k.zrodlo, Akcja::SIEC_PANELU_ANULUJ};  // tam, skąd uruchomiono (R-KONF-29)
      if (p == Przycisk::SRODEK) return dlugo ? Przejscie{e, Akcja::SIEC_PANELU_START} : Przejscie{Ekran::DZIEN, Akcja::SIEC_PANELU_ANULUJ};
      return nic;
  }
  return nic;
}

bool slotNocny(int m) { return m >= 23 * 60 + 30 || m < 5 * 60; }

namespace {
long czasCofniecia(Ekran e) {
  return e == Ekran::ZAPISANO ? 60 : (e == Ekran::PRZENIESIONE || e == Ekran::SPRZET_ZAPISANO) ? 30 : -1;
}
}  // namespace

long sekundDoKoncaCofniecia(Ekran e, long sek, int m) {
  long g = czasCofniecia(e);
  if (g < 0 || !slotNocny(m) || sek < 0 || sek >= g) return -1;
  return g - sek;
}

bool koniecCofnieciaTeraz(Ekran e, long sek, int m) {
  long g = czasCofniecia(e);
  return g >= 0 && slotNocny(m) && sek >= g && sek < g + KONIEC_COFNIECIA_TOL_S;
}

long sekundDoPowrotu(Ekran e, long sek, int m) {
  if (e == Ekran::OMOWIONE) return sek >= 60 ? 0 : 60 - sek;  // R-PRZ-05: także w nocy
  if (e == Ekran::RAPORT) return sek >= 20 ? 0 : 20 - sek;      // stan błędu raportu jak ekran przejściowy: 20 s
  if ((e == Ekran::PRZENIESIONE || e == Ekran::ZAPISANO || e == Ekran::SPRZET_ZAPISANO) && !slotNocny(m)) {
    long g = e == Ekran::ZAPISANO ? 60 : 30;  // R-MAS-01 30 s [PRÓG]
    return sek >= g ? 0 : g - sek;
  }
  if (e == Ekran::DZIEN || slotNocny(m)) return -1;
  long r = 300 - sek;
  return r < 0 ? 0 : r;
}
