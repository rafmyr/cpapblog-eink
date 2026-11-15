// SPDX-License-Identifier: GPL-3.0-or-later
// Kolejność stron kokpitu z telefonu a strona Portretu nocy pod ▶ (ADR-0045 pkt 2, R-KOK-34, R-POR-24, T-TEST-435;
// poprawka po przeglądzie): rano na ekranie problemu ▶ otwiera Portret, z niego ▶ otwiera PIERWSZĄ stronę
// ciągu kokpitu (domyślnie K1), ◀ z tej strony wraca na Portret; stopka Portretu nazywa tę stronę, a stopka pierwszej
// strony ma `◀ portret nocy`. Dalej ▶ prowadzi przez cały ciąg do U-01, a ◀ z pierwszej strony po powrocie z dalszej
// strony wraca zwykle (ekran dnia). Wszystkie 6 kolejności × strona sypialni z danymi i bez.
// Harness z test/testy_noc_pliki.cpp (dołączony, jego main() przemianowany); blokujący w tools/prototyp/testy.sh.
#define main main_testy_noc_pliki
#include "testy_noc_pliki.cpp"
#undef main

#include "nawigacja.h"
#include "sypialnia.h"
#include "uklad_ekranow.h"

static const char* nazwaEkranu(Ekran e) {
  const char* n = podpisStrony(e);
  return *n ? n : e == Ekran::PORTRET ? "portret" : "inny";
}

// wymaganie: R-KOK-34
// wymaganie: R-POR-24
static void testR_KOK_34_portretPierwszaStrona() {
  const StronaPrzestawna W[3] = {StronaPrzestawna::OSTATNIA_NOC, StronaPrzestawna::SIEDEM_NOCY, StronaPrzestawna::SYPIALNIA};
  int przypadkow = 0;
  for (int dane = 0; dane < 2; dane++) {
    int perm[3] = {0, 1, 2};
    do {
      UkladKokpitu u;
      for (int i = 0; i < 3; i++) u.kolejnosc[i] = W[perm[i]];
      const std::string kol = kolejnoscKokpituTekst(u);
      PanelN s("noc_zrodlo=plik\n" + (kol.empty() ? std::string() : "kokpit.kolejnosc=" + kol + "\n"));
      if (dane) {
        std::string t;
        for (int i = 0; i < 30; i++) t += zapiszSypialnie(NocSypialni{dodajDni("2026-11-04", i - 29), 20, 45, NAN}) + "\n";
        s.p.pliki.pliki["/sypialnia.csv"] = t;
      }
      // oczekiwany ciąg stron z numerem: kolejność z pliku, sypialnia tylko z danymi
      std::vector<Ekran> numerowane;
      for (StronaPrzestawna x : u.kolejnosc)
        if (x != StronaPrzestawna::SYPIALNIA || dane) numerowane.push_back(ekranStrony(x));
      const Ekran pierwsza = numerowane.front();
      const std::string opis = std::string(kol.empty() ? "domyślna" : kol) + (dane ? ", sypialnia z danymi" : ", bez sypialni");
      DobaAB d = dobaZWyciekiem(DZIS4, 0.5);  // ekran problemu S3 (E-23): Portret pod ▶ (R-POR-24)
      odpowiedzi(s, d);
      wybudz(s, ep(2026, 11, 4, 7, 0));
      SPRAWDZ(wLogu(s, "stopka: ▶ portret nocy (R-POR-24)"));
      uint32_t t = ep(2026, 11, 4, 7, 1);
      wybudzPrzyciskiem(s, t, Ekran::DZIEN, BIT_PRAWY);
      bool portret = s.p.start.stan.ekran == (uint8_t)Ekran::PORTRET;
      const std::string dalej = std::string("▶ ") + podpisStrony(pierwsza);
      bool stopkaPortretu = wLogu(s, "Portret nocy jako strona pod ▶ (R-POR-24)") && wLogu(s, "stopka " + dalej) &&
                            podpisWStopce(s.p.ekran.klatki.back(), dalej);
      // Portret ▶ → pierwsza strona ciągu ze stopką `◀ portret nocy`
      wybudzPrzyciskiem(s, t += 60, Ekran::PORTRET, BIT_PRAWY);
      bool naPierwsza = s.p.start.stan.ekran == (uint8_t)pierwsza;
      bool stopkaPierwszej = podpisWStopce(s.p.ekran.klatki.back(), "◀ portret nocy");
      // ◀ → Portret, ▶ → znów pierwsza
      wybudzPrzyciskiem(s, t += 60, pierwsza, BIT_LEWY);
      bool wrocil = s.p.start.stan.ekran == (uint8_t)Ekran::PORTRET;
      wybudzPrzyciskiem(s, t += 60, Ekran::PORTRET, BIT_PRAWY);
      SPRAWDZ(s.p.start.stan.ekran == (uint8_t)pierwsza);
      // ▶ kolejno przez cały ciąg do U-01; strony z numerem w kolejności z telefonu, Portret już nie wraca
      std::vector<Ekran> droga = {pierwsza};
      for (int i = 0; i < 12 && s.p.start.stan.ekran != (uint8_t)Ekran::USTAWIENIA; i++) {
        Ekran e = (Ekran)s.p.start.stan.ekran;
        wybudzPrzyciskiem(s, t += 60, e, BIT_PRAWY);
        droga.push_back((Ekran)s.p.start.stan.ekran);
      }
      std::vector<Ekran> numerowaneNaDrodze;
      bool bezPortretu = true;
      for (Ekran e : droga) {
        if (e == Ekran::K1 || e == Ekran::K2 || e == Ekran::SYPIALNIA) numerowaneNaDrodze.push_back(e);
        bezPortretu = bezPortretu && e != Ekran::PORTRET;
      }
      bool ciag = droga.back() == Ekran::USTAWIENIA && numerowaneNaDrodze == numerowane && bezPortretu;
      // druga strona: ◀ na pierwszą (bez `◀ portret nocy`), ◀ z niej na ekran dnia, nie na Portret
      Ekran druga = droga.size() > 1 ? droga[1] : Ekran::USTAWIENIA;
      PanelN z("noc_zrodlo=plik\n" + (kol.empty() ? std::string() : "kokpit.kolejnosc=" + kol + "\n"));
      if (dane) z.p.pliki.pliki["/sypialnia.csv"] = s.p.pliki.tresc("/sypialnia.csv");
      odpowiedzi(z, d);
      wybudz(z, ep(2026, 11, 4, 7, 0));
      uint32_t u2 = ep(2026, 11, 4, 7, 1);
      wybudzPrzyciskiem(z, u2, Ekran::DZIEN, BIT_PRAWY);
      wybudzPrzyciskiem(z, u2 += 60, Ekran::PORTRET, BIT_PRAWY);
      wybudzPrzyciskiem(z, u2 += 60, pierwsza, BIT_PRAWY);
      bool naDruga = z.p.start.stan.ekran == (uint8_t)druga && !podpisWStopce(z.p.ekran.klatki.back(), "◀ portret nocy");
      wybudzPrzyciskiem(z, u2 += 60, druga, BIT_LEWY);
      bool zPowrotem = z.p.start.stan.ekran == (uint8_t)pierwsza && !podpisWStopce(z.p.ekran.klatki.back(), "◀ portret nocy");
      wybudzPrzyciskiem(z, u2 += 60, pierwsza, BIT_LEWY);
      bool doDnia = z.p.start.stan.ekran == (uint8_t)Ekran::DZIEN;
      bool wszystko = portret && stopkaPortretu && naPierwsza && stopkaPierwszej && wrocil && ciag && naDruga && zPowrotem && doDnia;
      if (!wszystko) {
        printf("  USTERKA (%s): portret %d, stopka Portretu `%s` %d, pierwsza %s %d, `◀ portret nocy` %d, ◀ na Portret %d, "
               "ciąg %d, druga %s %d, z powrotem %d, ◀ na ekran dnia %d; droga:",
               opis.c_str(), portret, dalej.c_str(), stopkaPortretu, nazwaEkranu(pierwsza), naPierwsza, stopkaPierwszej,
               wrocil, ciag, nazwaEkranu(druga), naDruga, zPowrotem, doDnia);
        for (Ekran e : droga) printf(" %s", nazwaEkranu(e));
        printf("\n");
      }
      SPRAWDZ(wszystko);
      przypadkow++;
    } while (std::next_permutation(perm, perm + 3));
  }
  printf("  Portret ▶ → pierwsza strona ciągu, ◀ → Portret, ▶ … U-01: %d układów (6 kolejności × sypialnia z danymi i bez)\n",
         przypadkow);
}

int main() {
  setenv("TZ", STREFA_PANELU, 1);
  tzset();
  testR_KOK_34_portretPierwszaStrona();
  printf("testy_kolejnosc_portret: %d ok, %d błędów\n", ok, zle);
  return zle ? 1 : 0;
}
