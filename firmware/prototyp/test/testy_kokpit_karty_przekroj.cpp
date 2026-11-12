// SPDX-License-Identifier: GPL-3.0-or-later
// Testy przekrojowe po przeglądzie czytelności (10.11.2026): wzajemne oddziaływanie zmian (K2 ze zwiniętymi wierszami we wszystkich trybach i kombinacjach braków, droga do „Sprzętu” z kroku
// S-K-35 także w profilu publicznym, stopka karty bez zdania budzika, pusty ekran „Otwarte sprawy”, powód E-23).
// Każdy test, który pada, wypisuje swój temat. Uruchomienie: tools/prototyp/testy.sh.
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>
#include "ekrany.h"
#include "kokpit.h"
#include "nawigacja.h"
#include "noc.h"
#include "slot_dzienny.h"
#include "sprawy.h"
#include "sprzet.h"
#include "wspolne.h"

static int ok = 0, zle = 0;
static void sprawdz(const char* nr, bool w, const std::string& opis) {
  if (w) { ok++; return; }
  zle++;
  printf("BŁĄD %s: %s\n", nr, opis.c_str());
}

struct PlotnoT : Plotno {
  std::vector<unsigned char> px = std::vector<unsigned char>(800 * 480, 0);
  void piksel(int x, int y, bool c) override {
    if (x >= 0 && x < 800 && y >= 0 && y < 480) px[(size_t)y * 800 + (size_t)x] = c;
  }
  int czarne(int x0, int y0, int x1, int y1) const {
    int n = 0;
    for (int y = y0; y < y1; y++)
      for (int x = x0; x < x1; x++) n += px[(size_t)y * 800 + (size_t)x];
    return n;
  }
};

static const std::string DZIS = "2026-11-12";
static const char* const TRYBY[4] = {"CPAP/9///", "AutoSet/5/13//", "ASV/8///", "ASVAuto/6/10/3/15"};

static Noc pelna(const std::string& d, const char* ust) {
  Noc n;
  n.data = d;
  n.m01 = 420;
  n.wazna = true;
  n.ahi = 1.0f;
  n.oa = 0.4f;
  n.ca = 0.3f;
  n.h = 0.3f;
  n.cmed = 9.0f;
  n.cp95 = 10.5f;
  n.wmed = 4.0f;
  n.wp95 = 10.0f;
  n.emed = 7.0f;
  n.ep95 = 8.0f;
  n.psmed = 5.0f;
  n.psp95 = 7.0f;
  n.m13 = 14.0f;
  n.m17 = 1;
  n.ustawienia = ust;
  n.zrodlo = 'A';
  return n;
}

// (R-KOK-18, wszystkie tryby, każda kombinacja braków metryk w 30 nocach): informacja nie znika (wiersz jest
// w tabeli albo z nazwy w linii `brak danych: …`), linia w 744 px, tabela i legenda nad stopką, wiersz składowej
// AHI (wcięty) nie zostaje bez wiersza AHI nad sobą, wiersz w stanie odchylenia nigdy nie jest zwinięty.
static void testK2Kombinacje() {
  int kombinacji = 0, zwinietych = 0, najnizej = -1, sieroty = 0;
  std::string przyklad;
  for (const char* ust : TRYBY) {
    for (int maska = 0; maska < 256; maska++) {
      std::vector<Noc> h;
      for (int i = 0; i < 30; i++) {
        Noc n = pelna(dodajDni(DZIS, i - 29), ust);
        if (maska & 1) n.oa = NAN;
        if (maska & 2) n.ca = NAN;
        if (maska & 4) n.h = NAN;
        if (maska & 8) n.m17 = -1;
        if (maska & 16) n.m13 = NAN;
        if (maska & 32) n.psmed = NAN;
        if (maska & 64) { n.ep95 = NAN; n.cp95 = NAN; }
        if (maska & 128) n.ahi = NAN;
        h.push_back(n);
      }
      DaneK2 d = budujK2(h, DZIS, Norma{}, {});
      bool zwin[DaneK2::WIERSZE] = {};
      std::string l = zwinieteK2(d, zwin);
      kombinacji++;
      char opis[160];
      snprintf(opis, sizeof opis, "tryb %s, maska braków %d", ust, maska);
      sprawdz("K2: zwinięte wiersze", l.empty() || szer(F_P22B, l) <= 744, std::string(opis) + ": linia za szeroka: " + l);
      bool ahiWidoczny = true;
      for (int r = 0; r < d.wierszy; r++) {
        const WierszK2& w = d.w[r];
        if (zwin[r]) {
          zwinietych++;
          sprawdz("K2: zwinięte wiersze", !w.stan, std::string(opis) + ": zwinięty wiersz w stanie odchylenia: " + w.etykieta);
          sprawdz("K2: zwinięte wiersze", l.find(w.nazwaBraku) != std::string::npos,
                  std::string(opis) + ": zwinięty wiersz bez nazwy w linii: " + w.etykieta);
          if (std::string(w.etykieta) == "AHI") ahiWidoczny = false;
        } else if (w.wciecie) {
          if (!ahiWidoczny && sieroty++ == 0) przyklad = std::string(opis) + ", wiersz " + w.etykieta;
        }
      }
      PlotnoT p;
      rysujK2(p, d);
      // tabela i legenda kończą się nad kreską stopki (434): najniższy czarny wiersz nad nią z odstępem
      int dol = -1;
      for (int y = 433; y >= 380 && dol < 0; y--)
        if (p.czarne(0, y, 800, y + 1)) dol = y;
      if (dol > najnizej) najnizej = dol;
      sprawdz("K2: zwinięte wiersze", dol < 432, std::string(opis) + ": tabela albo legenda dotyka stopki (y " + std::to_string(dol) + ")");
    }
  }
  // AHI bez wartości przy składowych z wartościami (dane mało realne): AHI zostaje w tabeli, żeby wcięte
  // `obturacyjne`, `centralne`, `spłycenia` nie stały pod wierszem `Czas z maską` bez rodzica (R-KOK-18)
  sprawdz("AHI nad składowymi", sieroty == 0, std::to_string(sieroty) + " wciętych wierszy bez AHI nad sobą, np. " + przyklad);
  printf("  K2: %d kombinacji, %d zwiniętych wierszy razem, najniższy piksel treści nad stopką y %d\n", kombinacji, zwinietych, najnizej);
}

// (krok S-K-35, R-MAS-18, ADR-0041): S-K-35 nie prowadzi do strony „Sprzęt” (wycofanej), zapis jest na samym ekranie:
// ◀ 2 s = „wymienione dziś” elementu z ekranu (kontekst `sk31`) w obu profilach, ze sprawami i bez; krótkie ◀ nic,
// ↻ 2 s nic nie zapisuje; ▶ z ekranu dnia nigdy nie dochodzi do S8 ani „Sprzęt”.
static void testDrogaSK35() {
  sprawdz("krok S-K-35", numerStronyTekst(Ekran::SPRZET).empty(), "dawna strona „Sprzęt” ma numer strony");
  for (int pub = 0; pub < 2; pub++)
    for (int sprawy = 0; sprawy < 2; sprawy++) {
      KontekstNaw k;
      k.profilPubliczny = pub;
      k.sprawyDostepne = sprawy;
      std::string o = std::string(pub ? "profil publiczny" : "profil prywatny") + (sprawy ? ", ze sprawami" : ", bez spraw");
      Ekran e = Ekran::DZIEN;
      for (int krokow = 0; krokow < 12; krokow++) {
        sprawdz("krok S-K-35", e != Ekran::SPRZET && e != Ekran::NORMA, o + ": ▶ dochodzi do S8 albo „Sprzęt”");
        Przejscie p = nacisniecie(e, Przycisk::PRAWY, false, k);
        if (p.ekran == e) break;
        e = p.ekran;
      }
      k.sk31 = true;  // ekran dnia to S-K-35 (albo S-K-31): element zapisany przy rysowaniu
      Przejscie zap = nacisniecie(Ekran::DZIEN, Przycisk::LEWY, true, k);
      sprawdz("krok S-K-35", zap.akcja == Akcja::SPRZET_WYMIENIONE && zap.ekran == Ekran::SPRZET_ZAPISANO,
              o + ": ◀ 2 s na S-K-35 nie zapisuje wymiany");
      sprawdz("krok S-K-35", nacisniecie(Ekran::DZIEN, Przycisk::LEWY, false, k).akcja == Akcja::NIC,
              o + ": krótkie ◀ na S-K-35 coś robi");
      sprawdz("krok S-K-35", nacisniecie(Ekran::DZIEN, Przycisk::SRODEK, true, k).akcja == Akcja::NIC,
              o + ": ↻ 2 s na S-K-35 coś zapisuje");
    }
}

// (stopka karty bez zdania budzika): akcja z prawej (`↻ ponów`, `▶ kokpit`) nigdy nie ustępuje pogodzie z
// lewej; lewa ustępuje w całości tylko wtedy, gdy obie nie mieszczą się w 744 px.
static void testStopkaKarty() {
  const char* prawe[] = {"↻ ponów", "▶ kokpit", "przy łóżku 21° · 45% · CO2 1200"};
  for (const char* pr : prawe) {
    TrescEkranu t;
    t.tytul = "Dobra noc";
    t.stopkaLewo = "Bielsko-Biała 12° · przelotne opady deszczu ze śniegiem i wiatr";
    t.stopkaPrawo = pr;
    PlotnoT p;
    rysujKarte(p, {"cz 30.11", ""}, t, "");
    int xp = 772 - szer(F_P22B, pr);
    sprawdz("stopka karty", p.czarne(xp - 40, 444, 800, 474) > 30, std::string("prawa część stopki karty zniknęła: ") + pr);
  }
}

// (ADR-0029 pkt 7): pusty ekran „Otwarte sprawy”: `Brak otwartych spraw.` tylko z oceną `pełna`; każda inna
// ocena mówi o ograniczeniu (wiersz `Ocena:` inny niż `pełna`).
static void testPusteSprawy() {
  for (int pelnaOc = 0; pelnaOc < 2; pelnaOc++)
    for (int niep = 0; niep < 2; niep++) {
      sprawy::WejscieSpraw w;
      w.ocenaPelna = pelnaOc;
      w.ostatniaNiepewna = niep;
      sprawy::KartaSprawy k = sprawy::kartaPusta(w);
      bool brak = k.tytul == "Brak otwartych spraw.";
      sprawdz("pusty ekran „Otwarte sprawy”", brak == (k.ocena == "pełna"), "tytuł `" + k.tytul + "` przy ocenie `" + k.ocena + "`");
      sprawdz("pusty ekran „Otwarte sprawy”", k.ocena == "pełna" || k.tytul.find("Nie widzę") == 0, "ocena niepełna bez `Nie widzę`");
    }
}

// (E-23, P95): powód z najdłuższą wartością mieści się w 2 liniach ekranu zdarzenia (P28B, 744 px)
static void testPowodE23() {
  for (const char* x : {"38,0", "99,9", "120,0"}) {
    std::string s = std::string(x) + " l/min lub więcej przez 5% czasu terapii, próg 27.";
    sprawdz("powód E-23", zawin(F_P28B, s, 744).size() <= 2, "powód E-23 za długi: " + s);
  }
}

int main() {
  testK2Kombinacje();
  testDrogaSK35();
  testStopkaKarty();
  testPusteSprawy();
  testPowodE23();
  printf("testy_kokpit_karty_przekroj: %d ok, %d błędów\n", ok, zle);
  return zle ? 1 : 0;
}
