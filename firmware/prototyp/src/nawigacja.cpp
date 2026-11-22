// SPDX-License-Identifier: GPL-3.0-or-later
#include "nawigacja.h"
#include <cstring>

namespace {
Przejscie pokaz(Ekran e) { return {e, Akcja::POKAZ}; }

bool sypialniaWCiagu = false;  // R-KOK-29: ustawia dyrygent (ustawSypialnieWKokpicie()), dane z 30 nocy
UkladKokpitu uklad;            // R-KOK-34: ustawia dyrygent z konfiguracji (ustawUkladKokpitu())

bool permutacja(const UkladKokpitu& u) {
  bool byla[STRON_PRZESTAWNYCH] = {};
  for (StronaPrzestawna s : u.kolejnosc) {
    int i = static_cast<int>(s);
    if (i < 0 || i >= STRON_PRZESTAWNYCH || byla[i]) return false;
    byla[i] = true;
  }
  return true;
}

// Przejście na stronę `cel` ciągu: na „Otwarte sprawy” od pierwszej karty (▶) albo od ostatniej (◀, R-KOK-20).
Przejscie naStrone(Ekran cel, bool wstecz) {
  if (cel == Ekran::SPRAWY) return {Ekran::SPRAWY, wstecz ? Akcja::SPRAWY_OD_KONCA : Akcja::SPRAWY_OTWORZ};
  return pokaz(cel);
}

// ciąg stron kokpitu (5.5.7, R-KOK-19, R-KOK-29, R-KOK-34): ▶ = strona po `e`, ◀ = strona przed `e` w ciagKokpitu();
// „Co znaczą te liczby” tylko w profilu publicznym (R-KOK-22), „Otwarte sprawy” tylko przy co najmniej jednej sprawie
// (R-KOK-20), „Sypialnia · 30 nocy” tylko z danymi sypialni i bez ukrycia (R-KOK-29, R-KOK-34), inaczej pomijane
// w obie strony.
Przejscie stronaPo(Ekran e, const KontekstNaw& k) {
  switch (e) {
    case Ekran::USTAWIENIA: return pokaz(Ekran::FORMULARZ);
    case Ekran::FORMULARZ: return pokaz(Ekran::TEST);
    default: break;
  }
  std::vector<Ekran> c = ciagKokpitu(k, e);
  if (e == Ekran::DZIEN) return naStrone(c.front(), false);
  for (size_t i = 0; i + 1 < c.size(); i++)
    if (c[i] == e) return naStrone(c[i + 1], false);
  return {e, Akcja::NIC};
}

Przejscie stronaPrzed(Ekran e, const KontekstNaw& k) {
  if (e == Ekran::FORMULARZ || e == Ekran::TEST) return pokaz(Ekran::USTAWIENIA);  // oba to ekrany Ustawień: ◀ U-01
  std::vector<Ekran> c = ciagKokpitu(k, e);
  for (size_t i = 0; i < c.size(); i++)
    if (c[i] == e) return i == 0 ? pokaz(Ekran::DZIEN) : naStrone(c[i - 1], true);
  return {e, Akcja::NIC};
}
}  // namespace

bool UkladKokpitu::domyslny() const {
  UkladKokpitu d;
  for (int i = 0; i < STRON_PRZESTAWNYCH; i++)
    if (kolejnosc[i] != d.kolejnosc[i]) return false;
  return sypialnia == d.sypialnia;
}
void ustawUkladKokpitu(const UkladKokpitu& u) { uklad = permutacja(u) ? u : UkladKokpitu(); }
const UkladKokpitu& ukladKokpitu() { return uklad; }
Ekran ekranStrony(StronaPrzestawna s) {
  return s == StronaPrzestawna::OSTATNIA_NOC ? Ekran::K1 : s == StronaPrzestawna::SIEDEM_NOCY ? Ekran::K2 : Ekran::SYPIALNIA;
}

std::vector<Ekran> ciagKokpitu(const KontekstNaw& k, Ekran wymus) {
  std::vector<Ekran> c;
  for (StronaPrzestawna s : uklad.kolejnosc) {
    Ekran e = ekranStrony(s);
    if (e == Ekran::SYPIALNIA && (!k.sypialniaDostepna || !uklad.sypialnia) && wymus != Ekran::SYPIALNIA) continue;
    c.push_back(e);
    if (e != Ekran::K2) continue;
    // strony bez numeru zawsze zaraz po K2 (objaśnienie liczb K1 i K2, sprawy terapii; ADR-0044 „Odrzucone warianty”)
    if (k.profilPubliczny || wymus == Ekran::LICZBY) c.push_back(Ekran::LICZBY);
    if (k.sprawyDostepne || wymus == Ekran::SPRAWY) c.push_back(Ekran::SPRAWY);
  }
  c.push_back(Ekran::USTAWIENIA);
  return c;
}

Ekran pierwszaStronaKokpitu(const KontekstNaw& k) { return ciagKokpitu(k).front(); }

const char* podpisStrony(Ekran e) {
  switch (e) {
    case Ekran::DZIEN: return "ekran dnia";
    case Ekran::K1: return "ostatnia noc";
    case Ekran::K2: return "7 nocy";
    case Ekran::LICZBY: return "liczby";
    case Ekran::SPRAWY: return "sprawy";
    case Ekran::SYPIALNIA: return "sypialnia";
    case Ekran::USTAWIENIA: return "ustawienia";
    default: return "";
  }
}

std::string podpisCelu(Ekran e, bool dalej, const KontekstNaw& k) {
  // strona Portretu pod ▶ (R-POR-24): poza ciągiem, ▶ na pierwszą stronę ciągu, ◀ na ekran dnia
  if (e == Ekran::PORTRET) return podpisStrony(dalej ? pierwszaStronaKokpitu(k) : Ekran::DZIEN);
  std::vector<Ekran> c = ciagKokpitu(k, e);
  for (size_t i = 0; i < c.size(); i++) {
    if (c[i] != e) continue;
    if (!dalej) return podpisStrony(i == 0 ? Ekran::DZIEN : c[i - 1]);
    return i + 1 < c.size() ? podpisStrony(c[i + 1]) : "";
  }
  return "";
}

KontekstNaw kontekstStopki(bool sprawy, bool liczby, bool sypialnia) {
  KontekstNaw k;
  k.sprawyDostepne = sprawy;
  k.profilPubliczny = liczby;
  k.sypialniaDostepna = sypialnia;
  return k;
}

void ustawSypialnieWKokpicie(bool jest) { sypialniaWCiagu = jest; }
bool sypialniaWKokpicie() { return sypialniaWCiagu && uklad.sypialnia; }
int stronKokpitu() { return sypialniaWKokpicie() ? STRON_KOKPITU + 1 : STRON_KOKPITU; }

int numerStrony(Ekran e) {
  if (e == Ekran::USTAWIENIA) return stronKokpitu();  // zawsze ostatnia strona z numerem
  int n = 0;
  for (StronaPrzestawna s : uklad.kolejnosc) {
    Ekran x = ekranStrony(s);
    if (x == Ekran::SYPIALNIA && !sypialniaWKokpicie()) continue;  // poza ciągiem bez numeru (i tak nieosiągalna)
    n++;
    if (x == e) return n;
  }
  return 0;
}

bool stronaKokpitu(Ekran e) {
  return numerStrony(e) > 0 || e == Ekran::LICZBY || e == Ekran::SPRAWY || e == Ekran::SYPIALNIA;
}

namespace {
// odstęp: zwykła spacja albo twarda spacja U+00A0 (typografia polska); zwraca długość w bajtach albo 0
size_t odstepW(const std::string& t, size_t i) {
  if (i < t.size() && t[i] == ' ') return 1;
  if (i + 1 < t.size() && (unsigned char)t[i] == 0xC2 && (unsigned char)t[i + 1] == 0xA0) return 2;
  return 0;
}
size_t odstepPrzed(const std::string& t, size_t i) {  // odstęp kończący się tuż przed bajtem i
  if (i >= 1 && t[i - 1] == ' ') return 1;
  if (i >= 2 && (unsigned char)t[i - 2] == 0xC2 && (unsigned char)t[i - 1] == 0xA0) return 2;
  return 0;
}
// słowo przed numerem strony (R-KOK-30): tylko te formy, których używają teksty panelu i zestawu
bool slowoStronyPrzed(const std::string& t, size_t koniec) {
  static const char* const SLOWA[] = {"strona", "stronie", "stroną", "strony", "stronę", "raport", "raporcie", "raportu"};
  for (const char* sl : SLOWA) {
    size_t n = std::strlen(sl);
    if (koniec < n || t.compare(koniec - n, n, sl) != 0) continue;
    if (koniec == n) return true;
    unsigned char c = (unsigned char)t[koniec - n - 1];  // początek słowa: nie litera ASCII i nie bajt UTF-8
    if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c >= 0x80)) return true;
  }
  return false;
}
}  // namespace

std::string numeryStronWTekscie(const std::string& t) {
  // numer z zestawu nazywa stronę (R-KOK-30, R-KOK-36): `1 / 3` K1, `2 / 3` K2, `3 / 3` U-01 (5.5.7)
  const int nowe[4] = {0, numerStrony(Ekran::K1), numerStrony(Ekran::K2), numerStrony(Ekran::USTAWIENIA)};
  const int wszystkich = stronKokpitu();
  if (wszystkich == STRON_KOKPITU && nowe[1] == 1 && nowe[2] == 2 && nowe[3] == 3) return t;  // układ jak w zestawie
  std::string o;
  o.reserve(t.size() + 8);
  for (size_t i = 0; i < t.size(); i++) {
    // `{słowo strony}{odstęp}{1|2|3}{odstęp}/{odstęp}3` bez cyfry i przecinka po `3`
    size_t op = odstepPrzed(t, i), a1 = 0, a2 = 0, kon = 0;
    bool wzor = t[i] >= '1' && t[i] <= '3' && op > 0 && slowoStronyPrzed(t, i - op) && (a1 = odstepW(t, i + 1)) > 0 &&
                i + 1 + a1 < t.size() && t[i + 1 + a1] == '/' && (a2 = odstepW(t, i + 2 + a1)) > 0 &&
                (kon = i + 2 + a1 + a2) < t.size() && t[kon] == '3' &&
                (kon + 1 >= t.size() || ((t[kon + 1] < '0' || t[kon + 1] > '9') && t[kon + 1] != ','));
    if (!wzor) {
      o += t[i];
      continue;
    }
    int nowy = nowe[t[i] - '0'];  // `3 / 3` to U-01, ostatnia strona (R-KOK-23)
    o += std::to_string(nowy) + t.substr(i + 1, a1 + 1 + a2) + std::to_string(wszystkich);
    i = kon;
  }
  return o;
}

size_t dlugoscKontroliZNumerem(const std::string& l) {
  static const std::string P = " Na kontroli: raport ";
  const size_t n = P.size() + 6;  // `{n} / {N}.`
  if (l.size() < n || l.compare(l.size() - n, P.size(), P) != 0) return 0;
  const char* k = l.c_str() + l.size() - 6;
  bool cyfry = k[0] >= '1' && k[0] <= '9' && k[4] >= '1' && k[4] <= '9';
  return cyfry && k[1] == ' ' && k[2] == '/' && k[3] == ' ' && k[5] == '.' ? n : 0;
}

// R-PRZ-20: także strona Portretu pod ▶ (R-POR-24): krótkie naciśnięcie na niej niczego nie zapisuje, więc szybkie ▶ ▶
// z ekranu problemu dochodzi do K1
bool zKolejkiWolno(Ekran e, bool dlugo) {
  return !dlugo && (e == Ekran::DZIEN || stronaKokpitu(e) || e == Ekran::PORTRET);
}

bool czystaNawigacja(Ekran przed, const Przejscie& pr) {
  // oba ekrany w ciągu stron (zKolejkiWolno() dla krótkiego naciśnięcia: ekran dnia, strony kokpitu, Portret)
  if (!zKolejkiWolno(przed, false) || !zKolejkiWolno(pr.ekran, false)) return false;
  switch (pr.akcja) {
    case Akcja::NIC:
    case Akcja::POKAZ:
    case Akcja::SPRAWY_OTWORZ:  // karta „Otwartych spraw”: tylko numer karty (`cpap/b5_karta`), bez skutku dla danych
    case Akcja::SPRAWY_DALEJ:
    case Akcja::SPRAWY_WSTECZ:
    case Akcja::SPRAWY_OD_KONCA: return true;
    default: return false;
  }
}

int przytnijKarte(int karta, int liczba) { return liczba < 1 ? 1 : karta < 1 ? 1 : karta > liczba ? liczba : karta; }

std::string numerStronyTekst(Ekran e) {
  int n = numerStrony(e);
  return n ? std::to_string(n) + " / " + std::to_string(stronKokpitu()) : std::string();
}

Przejscie nacisniecie(Ekran e, Przycisk p, bool dlugo, const KontekstNaw& k) {
  const Przejscie nic = {e, Akcja::NIC};
  const Przejscie dom = pokaz(Ekran::DZIEN);  // krótkie ↻ poza ekranem dnia (wyjątki: pytania, siatka)
  if (p == Przycisk::BRAK) return nic;
  switch (e) {
    case Ekran::DZIEN:
      // (R-KONF-07): bez sieci domowej ◀ 2 s uruchamia sieć panelu, także na ekranie nocnym
      if (p == Przycisk::LEWY && dlugo && k.bezKonfiguracji) return {Ekran::SIEC_PANELU, Akcja::SIEC_PANELU_START};
      if (k.nocny) return p == Przycisk::PRAWY && !dlugo ? stronaPo(e, k) : nic;  // S9: bez pobrania i pytania
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
        return stronaPo(e, k);  // pierwsza strona kokpitu (domyślnie K1, R-KOK-34)
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
    case Ekran::SPRAWY:  // R-EKR-47, R-KOK-20: ▶ następna karta, za ostatnią dalej w ciągu; ◀ poprzednia, przed pierwszą K2 albo „liczby”
      if (dlugo) return nic;
      if (p == Przycisk::SRODEK) return dom;
      if (p == Przycisk::LEWY) return k.sprawa <= 1 || k.spraw == 0 ? stronaPrzed(e, k) : Przejscie{e, Akcja::SPRAWY_WSTECZ};
      return k.sprawa < k.spraw ? Przejscie{e, Akcja::SPRAWY_DALEJ} : stronaPo(e, k);
    case Ekran::LICZBY:  // „Co znaczą te liczby” (R-KOK-22): ◀ K2, ▶ „Otwarte sprawy”, sypialnia albo U-01
    case Ekran::SYPIALNIA:  // „Sypialnia · 30 nocy” (R-KOK-29): ◀ sprawy, liczby albo K2, ▶ U-01; przytrzymania nic
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
    case Ekran::USTAWIENIA:  // U-01 (3 / 3, z sypialnią 4 / 4): ◀ sypialnia, sprawy, liczby albo K2, ◀ 2 s sieć panelu (U-02), ▶ nic, ▶ 2 s U-03, ↻ dom
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
