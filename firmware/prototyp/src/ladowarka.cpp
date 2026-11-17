// SPDX-License-Identifier: GPL-3.0-or-later
// Limit ładowania baterii na kablu (ADR-0046, R-DANE-66…R-DANE-68); opis układu i źródła w src/ladowarka.h.
#include "ladowarka.h"

#include <cstdio>
#include <string>

#include "adaptery.h"
#include "wspolne.h"

namespace ladowarka {

int napiecieVreg(int kod) {
  if (kod < 0 || kod > 31) return -1;
  if (kod >= KOD_VREG_MAX) return VREG_MAX_MV;  // [K]: 11000…11111 = 4,624 V
  if (kod == KOD_VREG_WYJATEK) return 4352;     // [K]: 01111 = 4,352 V
  return VREG_MIN_MV + kod * VREG_KROK_MV;
}

int kodVregNieWyzej(int mV) {
  int kod = -1;
  for (int k = 0; k <= KOD_VREG_MAX; k++)
    if (napiecieVreg(k) <= mV) kod = k;  // napięcie rośnie z kodem (także przy wyjątku 15: 4304 < 4352 < 4368)
  return kod;
}

bool limitWKonfiguracji(const std::string& konfiguracja) {
  std::string t = bezBom(konfiguracja);
  const std::string wzor = std::string(KLUCZ_KONFIGURACJI) + "=";
  bool wlaczony = false;
  size_t p = 0;
  while (p < t.size()) {
    size_t e = t.find('\n', p);
    std::string l = t.substr(p, e == std::string::npos ? std::string::npos : e - p);
    obetnijKoniec(l);
    if (l.compare(0, wzor.size(), wzor) == 0) wlaczony = l.substr(wzor.size()) == "tak";  // ostatni wiersz wygrywa
    if (e == std::string::npos) break;
    p = e + 1;
  }
  return wlaczony;
}

// ---------- pamięć zastanych w NVS ----------
static const char* const NVS_PRZESTRZEN = "cpap";
static const char* const NVS_KLUCZ = "lad_zast";
static const uint32_t ZNACZNIK = 0x5Au;

Zastane PamiecZastanychNvs::wczytaj() {
  Zastane z;
  uint32_t v = 0;
  const Nvs::Odczyt o = n_.liczbaUStan(NVS_PRZESTRZEN, NVS_KLUCZ, v);
  if (o == Nvs::Odczyt::BLAD) {  // błąd odczytu to nie „brak wpisu” (inaczej zKarty() nadpisałby prawdziwy wpis)
    z.blad = true;
    return z;
  }
  if (o == Nvs::Odczyt::BRAK) return z;
  if ((v >> 24) != ZNACZNIK || (v & 0x00FF0000u) != 0) return z;
  z.znane = true;
  z.reg04 = (uint8_t)(v & 0xFFu);
  z.reg05 = (uint8_t)((v >> 8) & 0xFFu);
  return z;
}

bool PamiecZastanychNvs::zapisz(const Zastane& z) {
  uint32_t v = (ZNACZNIK << 24) | ((uint32_t)z.reg05 << 8) | (uint32_t)z.reg04;
  n_.ustawLiczbeU(NVS_PRZESTRZEN, NVS_KLUCZ, v);
  return n_.liczbaU(NVS_PRZESTRZEN, NVS_KLUCZ, 0) == v;  // flash bywa zawodny po cichu: odczyt kontrolny
}

void PamiecZastanychNvs::usun() { n_.usun(NVS_PRZESTRZEN, NVS_KLUCZ); }

// ---------- decyzja ----------
const char* nazwa(Wynik w) {
  switch (w) {
    case Wynik::WYLACZONY: return "wylaczony";
    case Wynik::WYLACZONY_NIZEJ: return "wylaczony, VREG ponizej 4208 mV bez wpisu (bez zapisu; wlacz i wylacz opcje)";
    case Wynik::NIE_NA_KABLU: return "nie na kablu, bez zmian";
    case Wynik::BRAK_UKLADU: return "brak ukladu SY6974B, bez zapisu";
    case Wynik::BLAD_ODCZYTU: return "blad odczytu, bez zapisu";
    case Wynik::POZA_KARTA: return "prog ponizej najnizszego VREG, bez zapisu";
    case Wynik::USTAWIONY: return "limit juz ustawiony";
    case Wynik::NIZEJ_FABRYCZNIE: return "VREG nie wyzej niz limit, bez zapisu";
    case Wynik::BLAD_PAMIECI: return "blad NVS (zapis albo odczyt wpisu), bez zapisu do ukladu";
    case Wynik::USTAWIONO: return "limit ustawiono";
    case Wynik::PRZYWROCONO: return "przywrocono zastane";
    case Wynik::PRZYWROCONO_Z_KARTY: return "wpis uszkodzony (VREG powyzej 4208 mV albo ponizej limitu), VREG z karty";
    case Wynik::WPIS_NIEAKTUALNY: return "wpis nieaktualny (w ukladzie nie ma limitu), usuniety bez zapisu";
    case Wynik::BLAD_ZAPISU: return "blad zapisu do ukladu";
  }
  return "?";
}

static int kodVreg(uint8_t reg04) { return (reg04 & MASKA_VREG) >> PRZESUNIECIE_VREG; }
static uint8_t polaVreg(int kod) { return (uint8_t)((kod << PRZESUNIECIE_VREG) & MASKA_VREG); }

// Kod VREG równy bieżącemu kodowi limitu albo jednemu z kodów limitu tej i dawnych wersji (ladowarka.h KODY_LIMITU).
static bool kodLimitu(int kod, int kodBiezacy) {
  if (kod == kodBiezacy) return true;
  for (int k : KODY_LIMITU)
    if (kod == k) return true;
  return false;
}

// Pole `maska` w rejestrze: odczyt, zmiana tylko bitów pola, zapis, odczyt kontrolny (cały bajt równy oczekiwanemu).
// Pole już równe celowi: bez zapisu.
static bool ustawPole(Rejestry& r, uint8_t rej, uint8_t maska, uint8_t wartosc, int& zapisow) {
  uint8_t przed = 0;
  if (!r.czytaj(rej, przed)) return false;
  uint8_t nowa = (uint8_t)((przed & (uint8_t)~maska) | (wartosc & maska));
  if (nowa == przed) return true;
  zapisow++;
  if (!r.zapisz(rej, nowa)) return false;
  uint8_t po = 0;
  return r.czytaj(rej, po) && po == nowa;
}

// ---------- twarda granica VREG (ADR-0046 pkt 5) ----------
// JEDYNA funkcja, która zapisuje pole VREG; każda ścieżka (ustawienie limitu, przywrócenie, cofnięcie po błędzie)
// przechodzi tędy, innego wywołania ustawPole(…, REJ_VREG, …) w pliku nie ma. Zapisany kod nigdy nie przekracza:
//  (a) KOD_VREG_DOMYSLNY (01011 = 4208 mV, wartość z karty dla ogniwa 4,2 V): kody 01100…11111, w tym wyjątek 01111
//      (4352 mV) i 11000…11111 (4624 mV), nie trafiają do układu nigdy, niezależnie od wpisu w NVS i stanu układu;
//  (b) kodu odczytanego z układu na początku tego wywołania (`kodWejscia`, przed pierwszą zmianą w tym wybudzeniu),
//      z jednym wyjątkiem: przywrócenie po wyłączeniu opcji, gdy na wejściu w układzie stał kod limitu (`kodLim` albo
//      kod z KODY_LIMITU); wtedy granicą jest tylko (a).
// Cel ponad granicą jest obcinany do granicy, czyli zawsze w dół. Napięcie rośnie z kodem (także przy wyjątku 01111:
// 4304 < 4352 < 4368 mV), więc porównanie kodów jest porównaniem napięć.
static bool zapiszVreg(Rejestry& r, int kodCelu, int kodWejscia, int kodLim, bool przywracanie, int& zapisow) {
  if (kodCelu < 0 || kodWejscia < 0) return false;
  int granica = KOD_VREG_DOMYSLNY;
  const bool podniesienieDozwolone = przywracanie && kodLimitu(kodWejscia, kodLim);
  if (!podniesienieDozwolone && kodWejscia < granica) granica = kodWejscia;
  const int kod = kodCelu < granica ? kodCelu : granica;
  return ustawPole(r, REJ_VREG, MASKA_VREG, polaVreg(kod), zapisow);
}

// Stan po wywołaniu do wyniku (do logu); bez odczytu = -1.
static void stanPo(Rejestry& r, WynikLimitu& w) {
  uint8_t a = 0, b = 0;
  if (r.czytaj(REJ_VREG, a)) w.vregMv = napiecieVreg(kodVreg(a));
  if (r.czytaj(REJ_WATCHDOG, b)) w.watchdog = (b & MASKA_WATCHDOG) >> 4;
}

// Zastane, gdy nie da się ich odczytać (układ nosi podpis limitu, a NVS nie ma wpisu): VREG z karty (4208 mV), licznik
// watchdoga JAK TERAZ. Odczyt z jednego egzemplarza panelu 16.11.2026 (REG05=8F, REG02=88, REG06=62, REG09=00) pokazał układ w trybie
// host z licznikiem wyłączonym, prądem 480 mA i VINDPM 4,1 V od wcześniejszego oprogramowania; włączony licznik po 40 s
// przywróciłby wszystkie wartości domyślne [S], w tym prąd 2040 mA, więc firmware licznika nigdy nie włącza.
static Zastane zKarty(uint8_t reg04, uint8_t reg05) {
  Zastane z;
  z.znane = true;
  z.reg04 = (uint8_t)((reg04 & (uint8_t)~MASKA_VREG) | polaVreg(KOD_VREG_DOMYSLNY));
  z.reg05 = reg05;
  return z;
}

WynikLimitu limitLadowania(Rejestry& r, PamiecZastanych& p, bool wlaczony, bool naKablu, int limitMv) {
  WynikLimitu w;
  uint8_t id = 0, reg04 = 0, reg05 = 0;
  if (!r.czytaj(REJ_ID, id) || (id & MASKA_PN) != PN_SY6974B) {
    w.wynik = Wynik::BRAK_UKLADU;
    return w;
  }
  if (!r.czytaj(REJ_VREG, reg04) || !r.czytaj(REJ_WATCHDOG, reg05)) {
    w.wynik = Wynik::BLAD_ODCZYTU;
    return w;
  }
  const int kodTeraz = kodVreg(reg04), kodLim = kodVregNieWyzej(limitMv);
  const uint8_t wdTeraz = (uint8_t)(reg05 & MASKA_WATCHDOG);
  w.vregMv = napiecieVreg(kodTeraz);
  w.watchdog = wdTeraz >> 4;
  if (kodLim < 0) {
    w.wynik = Wynik::POZA_KARTA;
    return w;
  }
  Zastane z = p.wczytaj();
  if (z.blad) {
    // nie wiadomo, czy wpis jest, więc żadnej decyzji: zero zapisów do układu i do NVS;
    // następne wybudzenie próbuje od nowa
    w.wynik = Wynik::BLAD_PAMIECI;
    return w;
  }
  const bool nasz = kodLimitu(kodTeraz, kodLim);              // w układzie stoi limit (ten albo dawnej wersji)
  const bool podpis = nasz && wdTeraz == WATCHDOG_WYLACZONY;  // limit z wyłączonym licznikiem

  if (!wlaczony) {
    if (!z.znane) {
      // bez wpisu zero zapisów zawsze; obniżony VREG tylko w logu (zdejmuje go włączenie i wyłączenie opcji)
      w.wynik = kodTeraz < KOD_VREG_DOMYSLNY ? Wynik::WYLACZONY_NIZEJ : Wynik::WYLACZONY;
      return w;
    }
    if (!nasz) {
      // w układzie nie ma limitu (reset, ktoś inny przestawił): wpis nieaktualny, nic z niego nie piszemy
      p.usun();
      w.wynik = Wynik::WPIS_NIEAKTUALNY;
      return w;
    }
    // w układzie stoi nasz limit, wpis aktualny. Przywracamy tylko VREG; licznika nie włączamy (ADR-0046 pkt 4).
    // Wpis z kodem powyżej 01011 albo poniżej kodu limitu to wpis uszkodzony albo obcy: z wpisu nic nie zapisujemy,
    // VREG = 4208 mV z karty (ADR-0046 pkt 7).
    const int kodZ = kodVreg(z.reg04);
    // wpis z kodem poniżej kodu limitu też jest uszkodzony (firmware zapisuje wpis tylko
    // wtedy, gdy układ ma VREG wyżej niż limit, albo z karty); dosłowne przywrócenie zostawiłoby baterię na stałe poniżej
    // limitu
    const bool uszkodzony = kodZ > KOD_VREG_DOMYSLNY || kodZ < kodLim;
    const int cel = uszkodzony ? KOD_VREG_DOMYSLNY : kodZ;
    if (!zapiszVreg(r, cel, kodTeraz, kodLim, true, w.zapisow)) {
      w.wynik = Wynik::BLAD_ZAPISU;  // wpis zostaje: następne wybudzenie próbuje ponownie
      stanPo(r, w);
      return w;
    }
    p.usun();
    w.wynik = uszkodzony ? Wynik::PRZYWROCONO_Z_KARTY : Wynik::PRZYWROCONO;
    stanPo(r, w);
    return w;
  }

  if (!naKablu) {
    w.wynik = Wynik::NIE_NA_KABLU;
    return w;
  }
  // podpis limitu bez wpisu (NVS wyczyszczona albo limit z dawnej wersji): wpis z VREG z karty, żeby wyłączenie opcji
  // mogło limit zdjąć; tylko NVS, bez zapisu do układu
  if (podpis && !z.znane) {
    Zastane zk = zKarty(reg04, reg05);
    if (!p.zapisz(zk)) {
      w.wynik = Wynik::BLAD_PAMIECI;
      return w;
    }
    z = zk;
  }
  if (kodTeraz <= kodLim) {
    // VREG już nie wyżej niż limit: nigdy nie podnosimy
    if (kodTeraz == kodLim && wdTeraz != WATCHDOG_WYLACZONY && z.znane) {
      // VREG ustawiony przez nas, licznik jeszcze włączony (przerwane wybudzenie): dokończ wyłączeniem licznika
      if (!ustawPole(r, REJ_WATCHDOG, MASKA_WATCHDOG, WATCHDOG_WYLACZONY, w.zapisow)) {
        w.wynik = Wynik::BLAD_ZAPISU;
        stanPo(r, w);
        return w;
      }
      w.wynik = Wynik::USTAWIONO;
      stanPo(r, w);
      return w;
    }
    if (podpis) {
      w.wynik = Wynik::USTAWIONY;
      return w;
    }
    if (z.znane && !nasz) p.usun();  // układ niżej niż limit, nie od nas: wpis nieaktualny
    w.wynik = Wynik::NIZEJ_FABRYCZNIE;
    return w;
  }
  // VREG wyżej niż limit: obniżamy. Zastane = bieżące rejestry; wpis zostaje tylko wtedy, gdy w układzie stoi limit
  // dawnej wersji. Wpis różny od układu bez limitu jest nieaktualny (reset, ktoś inny): zastępujemy go bieżącymi.
  if (!z.znane || (!nasz && (z.reg04 != reg04 || z.reg05 != reg05))) {
    Zastane b;
    b.znane = true;
    b.reg04 = reg04;
    b.reg05 = reg05;
    if (!p.zapisz(b)) {
      w.wynik = Wynik::BLAD_PAMIECI;
      return w;
    }
    z = b;
  }
  // najpierw VREG (cel = limit, tu kodTeraz > kodLim, więc to obniżenie; granica w zapiszVreg i tak go pilnuje): gdy
  // licznik jest włączony, przerwanie po tym kroku kończy się powrotem układu do wartości domyślnych po jego upływie [S]
  if (!zapiszVreg(r, kodLim, kodTeraz, kodLim, false, w.zapisow)) {
    w.wynik = Wynik::BLAD_ZAPISU;
    stanPo(r, w);
    return w;
  }
  if (!ustawPole(r, REJ_WATCHDOG, MASKA_WATCHDOG, WATCHDOG_WYLACZONY, w.zapisow)) {
    int z2 = 0;
    // próba powrotu do VREG sprzed tego wywołania, przez tę samą granicę: nigdy wyżej niż wejście ani 4208 mV (wejście
    // 4352/4624 mV wraca jako 4208 mV)
    zapiszVreg(r, kodTeraz, kodTeraz, kodLim, false, z2);
    w.zapisow += z2;
    w.wynik = Wynik::BLAD_ZAPISU;
    stanPo(r, w);
    return w;
  }
  w.wynik = Wynik::USTAWIONO;
  stanPo(r, w);
  return w;
}

std::string opis(const WynikLimitu& w) {
  char b[200];
  snprintf(b, sizeof b, "ladowanie: %s, VREG %d mV, watchdog %d, zapisow %d", nazwa(w.wynik), w.vregMv, w.watchdog,
           w.zapisow);
  return b;
}

}  // namespace ladowarka
