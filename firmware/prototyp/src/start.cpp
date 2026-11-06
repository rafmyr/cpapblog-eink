// SPDX-License-Identifier: GPL-3.0-or-later
// Siatka aktualizacji (src/start.h): licznik startów po błędzie, tryb awaryjny, samosprawdzenie, strażnik.
// Spec techniczna 7.8 („Powrót do poprzedniej wersji”, „Pętla restartów i zawieszenie”); testy: test/testy_wybudzenie.cpp.
#include "start.h"
#include <cerrno>
#include <cstdio>
#include <cstdlib>

namespace {

const uint32_t MAGIA_LICZNIKA = 0x5A7A0001;

// FNV-1a 32 bity
uint32_t fnv(uint32_t h, uint32_t v, int bajtow) {
  for (int i = 0; i < bajtow; i++) {
    h ^= (v >> (8 * i)) & 0xFFu;
    h *= 16777619u;
  }
  return h;
}

// Suma kontrolna pól licznika, pole po polu (bez bajtów wyrównania, które w pamięci RTC są przypadkowe).
uint32_t sumaLicznika(const LicznikStartow& l) {
  uint32_t h = 2166136261u;
  h = fnv(h, l.magia, 4);
  h = fnv(h, l.wersja, 4);
  h = fnv(h, l.poBledzie, 1);
  h = fnv(h, l.spadki, 2);
  return h ^ 0xA5A5A5A5u;
}

bool waznyDla(const LicznikStartow& l, uint32_t wersja) {
  return l.magia == MAGIA_LICZNIKA && l.wersja == wersja && l.suma == sumaLicznika(l);
}

void zamknij(LicznikStartow& l, uint32_t wersja) {
  l.magia = MAGIA_LICZNIKA;
  l.wersja = wersja;
  l.suma = sumaLicznika(l);
}

// Błąd programu albo strażnik: start po nieprawidłowym zakończeniu (R-AW-39, T-WYM-82).
bool poBledzieProgramu(PrzyczynaStartu p) {
  return p == PrzyczynaStartu::PANIKA || p == PrzyczynaStartu::TASK_WDT || p == PrzyczynaStartu::INT_WDT ||
         p == PrzyczynaStartu::WDT;
}

// Liczba bez znaku z tekstu w podstawie `podstawa`; cały tekst musi być liczbą.
bool liczba(const std::string& t, int podstawa, unsigned long& w) {
  if (t.empty() || t.size() > 10) return false;
  for (char c : t) {
    bool cyfra = (c >= '0' && c <= '9') || (podstawa == 16 && ((c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F')));
    if (!cyfra) return false;
  }
  char* koniec = nullptr;
  errno = 0;
  w = strtoul(t.c_str(), &koniec, podstawa);
  return errno == 0 && koniec && *koniec == '\0';
}

}  // namespace

// Układ tekstu: `v1;{wersja szesnastkowo};{przyczyna liczbą};{u06 0|1}`.
std::string zapiszZnacznik(const ZnacznikAwaryjny& z) {
  char b[48];
  snprintf(b, sizeof b, "v1;%08lx;%u;%d", (unsigned long)z.wersja, (unsigned)z.przyczyna, z.u06Narysowane ? 1 : 0);
  return b;
}

bool parsujZnacznik(const std::string& tekst, ZnacznikAwaryjny& z) {
  z = ZnacznikAwaryjny();
  std::string pola[4];
  size_t n = 0, a = 0;
  // ostatnie pole sięga końca tekstu (bez średnika i czegokolwiek po nim); wcześniej
  // `v1;…;1;śmieci` przechodziło, bo sprawdzenie szukało tylko kolejnego średnika.
  bool doKonca = false;
  while (n < 4) {
    size_t b = tekst.find(';', a);
    pola[n++] = tekst.substr(a, b == std::string::npos ? std::string::npos : b - a);
    if (b == std::string::npos) {
      doKonca = true;
      break;
    }
    a = b + 1;
  }
  if (n != 4 || !doKonca || pola[0] != "v1") return false;
  unsigned long wersja = 0, przyczyna = 0;
  if (!liczba(pola[1], 16, wersja) || wersja > 0xFFFFFFFFul) return false;
  if (!liczba(pola[2], 10, przyczyna) || przyczyna > (unsigned long)PrzyczynaStartu::INNA) return false;
  if (pola[3] != "0" && pola[3] != "1") return false;
  z.jest = true;
  z.wersja = (uint32_t)wersja;
  z.przyczyna = (PrzyczynaStartu)przyczyna;
  z.u06Narysowane = pola[3] == "1";
  return true;
}

uint32_t identyfikatorWersji(const char* wersja) {
  uint32_t h = 2166136261u;
  for (const char* c = wersja ? wersja : ""; *c; c++) h = fnv(h, (uint8_t)*c, 1);
  return h ? h : 1u;
}

std::string wersjaWydania(const char* identyfikator) {
  std::string w = identyfikator ? identyfikator : "";
  size_t p = w.find('+');
  return p == std::string::npos ? w : w.substr(0, p);
}

DecyzjaStartu decyzjaStartu(LicznikStartow& l, PrzyczynaStartu p, uint32_t wersja, const ZnacznikAwaryjny& z) {
  DecyzjaStartu d;
  if (!waznyDla(l, wersja) || p == PrzyczynaStartu::ZASILANIE) {
    l.poBledzie = 0;
    l.spadki = 0;
  }
  if (poBledzieProgramu(p) && l.poBledzie < 255) l.poBledzie++;
  if (p == PrzyczynaStartu::SPADEK) {
    if (l.spadki < 65535) l.spadki++;
    d.spadekNapiecia = true;
  }
  zamknij(l, wersja);
  d.poBledzie = l.poBledzie;
  d.spadki = l.spadki;

  if (z.jest && z.wersja != wersja) {  // R-AW-39: start innej wersji kończy tryb awaryjny
    d.usunZnacznik = true;
  } else if (z.jest) {  // znacznik tej wersji w NVS: dalej tryb awaryjny, bez drugiego zapisu
    d.awaryjny = true;
    d.rysujU06 = !(z.u06Narysowane && poBledzieProgramu(p));
    return d;
  }
  if (l.poBledzie >= STARTOW_DO_AWARYJNEGO) {
    d.awaryjny = true;
    d.rysujU06 = true;
    d.zapiszZnacznik = true;
    d.znacznik.jest = true;
    d.znacznik.wersja = wersja;
    d.znacznik.przyczyna = p;
    d.znacznik.u06Narysowane = true;
  }
  return d;
}

void zerujPrzedSnem(LicznikStartow& l, uint32_t wersja) {
  if (!waznyDla(l, wersja)) l.spadki = 0;
  l.poBledzie = 0;
  zamknij(l, wersja);
}

AkcjaAwaryjna nacisniecieAwaryjne(EkranAwaryjny e, Przycisk p, bool dlugo) {
  if (e == EkranAwaryjny::U06) {
    if (p == Przycisk::PRAWY) return AkcjaAwaryjna::TEST_SPRZETU;
    if (p == Przycisk::SRODEK && dlugo) return AkcjaAwaryjna::WYJDZ;
    return AkcjaAwaryjna::NIC;
  }
  if (p == Przycisk::LEWY && !dlugo) return AkcjaAwaryjna::WROC;
  if (p == Przycisk::SRODEK && !dlugo) return AkcjaAwaryjna::ODSWIEZ;
  return AkcjaAwaryjna::NIC;
}

void wyjscieZAwaryjnego(LicznikStartow& l, uint32_t wersja) { zerujPrzedSnem(l, wersja); }

TekstyU06 tekstyU06() {
  return {"Panel wymaga serwisu", "Oprogramowanie panelu nie uruchamia się poprawnie. Nocy nie oceniam.",
          "→ Daj znać osobie, która instalowała panel.", "▶ test sprzętu · ↻ 2 s spróbuj ponownie"};
}

PoSamosprawdzeniu decyzjaSamosprawdzenia(StanWersji s, const WynikSamosprawdzenia& w) {
  if (s != StanWersji::DO_SPRAWDZENIA) return PoSamosprawdzeniu::NIC;
  bool ok = w.plikiMontuja && w.zestawWczytany && w.migracjeOk && w.ekranRysuje && w.wifiLaczy;
  return ok ? PoSamosprawdzeniu::OZNACZ_DOBRA : PoSamosprawdzeniu::WROC_DO_POPRZEDNIEJ;
}

bool wolnoZasnac(StanWersji s) { return s != StanWersji::DO_SPRAWDZENIA; }

bool samosprawdzenieWymagane(StanWersji s) { return s == StanWersji::DO_SPRAWDZENIA; }

UstawieniaStraznika ustawieniaStraznika() { return {STRAZNIK_LIMIT_S, true}; }

DecyzjaStartu decyzjaStartu(LicznikStartow& l, PrzyczynaStartu p, uint32_t wersja, const ZnacznikAwaryjny& z,
                            StanWersji s) {
  if (s != StanWersji::DO_SPRAWDZENIA || !z.jest) return decyzjaStartu(l, p, wersja, z);
  ZnacznikAwaryjny innej = z;  // nowe wgranie: znacznik jak z innej wersji (usunąć, zwykły start)
  innej.wersja = wersja + 1u;
  return decyzjaStartu(l, p, wersja, innej);
}
