// SPDX-License-Identifier: GPL-3.0-or-later
// Przegląd, spec 5.4.17 R-EKR-67…R-EKR-69).
// Testy przegląd, pisane z kryteriów (spec, ADR-0043, ADR-0029 pkt 7, decyzja właściciela „brak ostatniej nocy
// po zamknięciu okna porannego”), nie z kodu autora. Recenzja opisała 8 usterek (w karcie zmiany,
// „”). Po poprawkach (14.11.2026) każda usterka jest zwykłym sprawdzeniem: plik jest
// blokujący w tools/prototyp/testy.sh (T-TEST-426); dopisane scenariusze dyrygenta: panel wyłączony 3 dni
// (zaległe doby = pole), U-05 przy zaległych dobach, strona „Sypialnia · 30 nocy” w kombinacjach,
// klepsydra w wariancie oszczędnym (liczby odświeżeń jak przed częścią C).
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <functional>
#include <string>
#include <vector>

#include "adaptery_atrapy.h"
#include "aktualizacja.h"
#include "airbridge.h"
#include "dyrygent.h"
#include "dzien.h"
#include "ekrany.h"
#include "kokpit.h"
#include "noc.h"
#include "plan.h"
#include "sekrety.h"
#include "sprawy.h"
#include "sypialnia_ekran.h"
#include "start.h"
#include "wspolne.h"
#include "wybudzenie.h"

static int ok = 0, zle = 0;
#define SPRAWDZ(w)                                                         \
  do {                                                                     \
    if (w) ok++;                                                           \
    else { zle++; printf("BŁĄD %s:%d: %s\n", __FILE__, __LINE__, #w); }   \
  } while (0)
// Dawna usterka recenzji (RB-xx), po poprawkach zwykłe sprawdzenie: niespełnione = błąd testu z identyfikatorem.
#define SPRAWDZ_RB(id, w, opis)                         \
  do {                                                 \
    if (w) ok++;                                       \
    else { zle++; printf("BŁĄD %s: %s\n", id, opis); } \
  } while (0)

static const char* STREFA = "CET-1CEST,M3.5.0,M10.5.0/3";

static uint32_t ep(int r, int mi, int d, int h, int m, int s = 0) {
  std::tm t{};
  t.tm_year = r - 1900;
  t.tm_mon = mi - 1;
  t.tm_mday = d;
  t.tm_hour = h;
  t.tm_min = m;
  t.tm_sec = s;
  t.tm_isdst = -1;
  return (uint32_t)mktime(&t);
}
static std::string dataLok(uint32_t t) {
  time_t tt = (time_t)t;
  std::tm l{};
  localtime_r(&tt, &l);
  char b[11];
  strftime(b, sizeof b, "%Y-%m-%d", &l);
  return b;
}
static int minutaLok(uint32_t t) {
  time_t tt = (time_t)t;
  std::tm l{};
  localtime_r(&tt, &l);
  return l.tm_hour * 60 + l.tm_min;
}

static std::vector<Noc> nocDo(const std::string& ostatnia, int n = 3) {
  std::vector<Noc> h;
  for (int i = n - 1; i >= 0; i--) {
    Noc x;
    x.data = dodajDni(ostatnia, -i);
    x.wazna = true;
    x.m01 = 420;
    x.ahi = 2.0f;
    h.push_back(x);
  }
  return h;
}

// ============================================================================================================
// (1) Definicja daneNieaktualne() / nocOczekiwana(): tabela przypadków w czasie × stan
// ============================================================================================================
// kontaktData, kontaktMinuta (nazwy przegląd): chwila, w której panel poznał stan nocy oczekiwanej
// (StanRtc::nocZnanaEp, po poprawkach nie sam kontakt z `/api/status`, tylko lista bez zaległych dób)
struct Przypadek {
  const char* opis;
  std::vector<Noc> hist;
  std::string dzis;
  int minuta;
  std::string kontaktData;
  int kontaktMinuta;
  bool oczekiwane;  // z kryteriów (decyzja właściciela + spec R-EKR-67)
};
static void testTabelaDefinicji() {
  printf("(1) tabela daneNieaktualne():\n");
  const std::vector<Noc> H13 = nocDo("2026-11-13"), H14 = nocDo("2026-11-14"), H12 = nocDo("2026-11-12"),
                         H10 = nocDo("2026-11-10"), PUSTA;
  std::vector<Noc> zJutra = nocDo("2026-11-12");  // noc z datą jutrzejszą przez błąd zegara
  Noc j;
  j.data = "2026-11-15";
  j.wazna = true;
  zJutra.push_back(j);
  std::vector<Przypadek> t = {
      // przed / na / po 11:00, noc z dziś jest
      {"noc dzisiejsza jest, 10:59", H14, "2026-11-14", 659, "2026-11-14", 400, false},
      {"noc dzisiejsza jest, 11:00, bez kontaktu po 11", H14, "2026-11-14", 660, "2026-11-14", 400, false},
      {"noc dzisiejsza jest, 23:59", H14, "2026-11-14", 1439, "", -1, false},
      // brak nocy z dziś, noc z wczoraj jest
      {"brak dziś, 10:59, kontakt 06:00", H13, "2026-11-14", 659, "2026-11-14", 360, false},
      {"brak dziś, 11:00, kontakt 06:00 (AirBridge milczy)", H13, "2026-11-14", 660, "2026-11-14", 360, true},
      {"brak dziś, 11:00, kontakt 11:00 (noc bez terapii)", H13, "2026-11-14", 660, "2026-11-14", 660, false},
      {"brak dziś, 14:30, kontakt 14:30 (noc bez terapii)", H13, "2026-11-14", 870, "2026-11-14", 870, false},
      {"brak dziś, 11:00, kontakt 10:59 (ostatnie przed 11)", H13, "2026-11-14", 660, "2026-11-14", 659, true},
      {"brak dziś, 11:00, nigdy kontaktu", H13, "2026-11-14", 660, "", -1, true},
      // tuż po północy (noc oczekiwana = wczoraj)
      {"00:05 15.11, brak 14.11, kontakt 14.11 14:00", H13, "2026-11-15", 5, "2026-11-14", 840, false},
      {"00:05 15.11, brak 14.11, kontakt 14.11 06:00", H13, "2026-11-15", 5, "2026-11-14", 360, true},
      {"00:05 15.11, jest 14.11", H14, "2026-11-15", 5, "", -1, false},
      // przełom miesiąca i roku
      {"01.12 10:59, jest 30.11", nocDo("2026-11-30"), "2026-12-01", 659, "", -1, false},
      {"01.12 11:00, jest 30.11, bez kontaktu", nocDo("2026-11-30"), "2026-12-01", 660, "", -1, true},
      {"01.01 10:59, jest 31.12", nocDo("2026-12-31"), "2027-01-01", 659, "", -1, false},
      {"01.01 11:00, jest 31.12, kontakt 31.12 20:00", nocDo("2026-12-31"), "2027-01-01", 660, "2026-12-31", 1200, true},
      {"01.01 08:00, jest 30.12, kontakt 31.12 09:00", nocDo("2026-12-30"), "2027-01-01", 480, "2026-12-31", 540, true},
      {"01.01 08:00, jest 30.12, kontakt 31.12 12:00", nocDo("2026-12-30"), "2027-01-01", 480, "2026-12-31", 720, false},
      // historia pusta, pierwszy start
      {"historia pusta, 08:00, bez kontaktu (pierwszy start rano)", PUSTA, "2026-11-14", 480, "", -1, true},
      {"historia pusta, 12:00, kontakt 12:00", PUSTA, "2026-11-14", 720, "2026-11-14", 720, false},
      // zegar nieznany
      {"zegar nieznany", H10, "", 720, "", -1, false},
      // panel wyłączony 3 dni, pierwszy kontakt po włączeniu 08:00 (noc 13.11 na AirBridge, jeszcze nie pobrana)
      {"wyłączony 3 dni, 14.11 08:00 przed odczytem", H10, "2026-11-14", 480, "2026-11-10", 700, true},
      ///sam odczyt /api/status przy zaległych dobach nie jest poznaniem stanu nocy (dyrygent nie zapisuje
      // chwili, zob. testWylaczonyTrzyDni()); chwila z wcześniejszej obserwacji (10.11) nie pokrywa nocy 13.11
      {"wyłączony 3 dni, 14.11 08:00 po odczycie /api/status (noce zaległe niepobrane)", H10, "2026-11-14", 480,
       "2026-11-10", 700, true},
      {"wyłączony 3 dni, zaległe pobrane, noc 13.11 bez terapii poznana 14.11 08:00", H12, "2026-11-14", 480,
       "2026-11-14", 480, false},
      // noc z datą jutrzejszą przez błąd zegara (ignorowana), bez kontaktu
      {"noc z jutra w historii (błąd zegara), 12:00, bez kontaktu", zJutra, "2026-11-14", 720, "", -1, true},
      // kontakt zapisany z zegarem w przyszłości (błąd zegara, potem korekta NTP), AirBridge potem milczy
      {"kontakt z przyszłości (20.11), 14.11 12:00, brak nocy", H13, "2026-11-14", 720, "2026-11-20", 600, true},
      // dokładnie 11:00 z kontaktem dokładnie 11:00 wczoraj (przed 11 dziś noc oczekiwana = wczoraj)
      {"14.11 10:00, jest 12.11, kontakt 13.11 11:00 (noc 13.11 bez terapii)", H12, "2026-11-14", 600, "2026-11-13", 660,
       false},
      {"14.11 10:00, jest 12.11, kontakt 13.11 10:59", H12, "2026-11-14", 600, "2026-11-13", 659, true},
  };
  int i = 0;
  for (auto& c : t) {
    bool w = daneNieaktualne(c.hist, c.dzis, c.minuta, c.kontaktData, c.kontaktMinuta);
    printf("  %2d. %-82s → %s (oczek. %s)%s\n", ++i, c.opis, w ? "POLE" : "bez pola", c.oczekiwane ? "POLE" : "bez pola",
           w == c.oczekiwane ? "" : "  <-- RÓŻNICA");
    if (std::string(c.opis).find("noce zaległe niepobrane") != std::string::npos ||
        std::string(c.opis).find("kontakt z przyszłości") != std::string::npos)
      SPRAWDZ_RB("", w == c.oczekiwane, c.opis);
    else
      SPRAWDZ(w == c.oczekiwane);
  }
  // nocOczekiwana: granica 11:00 i przełomy
  SPRAWDZ(nocOczekiwana("2026-11-14", 659) == "2026-11-13");
  SPRAWDZ(nocOczekiwana("2026-11-14", 660) == "2026-11-14");
  SPRAWDZ(nocOczekiwana("2027-01-01", 0) == "2026-12-31");
  SPRAWDZ(nocOczekiwana("2026-03-01", 100) == "2026-02-28");
  SPRAWDZ(nocOczekiwana("", 700).empty());
}

// ============================================================================================================
// (5) Belka: pełny przegląd kombinacji (ekran × stan × alarm × ocena × romb × klepsydra × wersja × bateria)
// ============================================================================================================
struct PlotnoR : Plotno {
  std::vector<unsigned char> px = std::vector<unsigned char>(800 * 480, 0);
  void piksel(int x, int y, bool c) override {
    if (x < 0 || y < 0 || x >= 800 || y >= 480) return;
    px[(size_t)(y * 800 + x)] = c ? 1 : 0;
  }
  bool cz(int x, int y) const { return px[(size_t)(y * 800 + x)] != 0; }
  int czarne(int x0, int y0, int x1, int y1) const {
    int n = 0;
    for (int y = y0; y < y1; y++)
      for (int x = x0; x < x1; x++) n += px[(size_t)(y * 800 + x)];
    return n;
  }
};
static BateriaEkranu bat(int p, bool kabel = false, bool niska = false) {
  BateriaEkranu b;
  b.procent = p;
  b.kabel = kabel;
  b.niska = niska;
  return b;
}
static void testKombinacjeBelki() {
  printf("(5) kombinacje belki:\n");
  const std::vector<BateriaEkranu> baterie = {bat(100), bat(78), bat(5, false, true), bat(-1), bat(60, true)};
  const char* dniT[] = {"pn", "wt", "śr", "cz", "pt", "sb", "nd"};
  const std::vector<std::string> daty = {"28.08", "03.03", "11.11", "30.12", "08.08"};
  long przyp = 0, poleZawsze = 0, nakladanie = 0, wersjaZnika = 0, klepsZnika = 0, wersjaZnikaBezKlepsydry = 0;
  long godzinaZnika = 0, slowoZnika = 0, nieaktNocy = 0, poleSypialni = 0, wersjaZnikaSypialni = 0, przypSyp = 0;
  long portretZData = 0;
  for (int ekr = 0; ekr < 5; ekr++)              // 0 ekran dnia, 1 Portret, 2 K1, 3 K2, 4 Sypialnia · 30 nocy
    for (int nieakt = 0; nieakt < 2; nieakt++)
      for (int ocena = 0; ocena < 3; ocena++)    // brak, częściowa, ograniczona
        for (int romb = 0; romb < 2; romb++)
          for (int kl = 0; kl < 2; kl++)
            for (int wer = 0; wer < 2; wer++)
              for (auto& b : baterie)
                for (auto& dt : daty)
                  for (int dz = 0; dz < 7; dz++) {
                    if (ekr != 2 && (romb || ocena == 1)) continue;  // tylko K1 ma romb i ocenę częściową
                    ustawBaterieEkranu(b);
                    ZnakiBelki z;
                    z.nieaktualne = nieakt;
                    z.daneZ = dt;
                    z.pobieram = kl;
                    z.nowaWersja = wer;
                    ustawZnakiBelki(z);
                    Belka bl;
                    std::string dzis = "2026-11-14";
                    std::vector<Noc> h = nieakt ? nocDo("2026-11-13") : nocDo("2026-11-14");
                    if (ocena == 2 && !nieakt) h.back().wp95 = 30.0f;  // noc niepewna (wyciek) przy ocenie ograniczonej
                    DaneDnia dd;
                    dd.dzis = dzis;
                    dd.hist = &h;
                    dd.godzinaImportu = "08:08";
                    if (ekr == 0) bl = belkaDnia(dd);
                    else if (ekr == 1) bl = belkaPortretu(dd);
                    else if (ekr == 2) {
                      DaneK1 k;
                      k.dataNocy = nieakt ? dt : dt;
                      k.dzienNocy = dniT[dz];
                      k.ubiegla = !nieakt;
                      k.godzinaDanych = nieakt ? "" : "08:08";
                      k.romb = romb;
                      k.ocena = ocena == 1 ? "ocena częściowa" : ocena == 2 ? "ocena ograniczona" : "";
                      bl = belkaK1(k);
                    } else if (ekr == 3) {
                      DaneK2 k;
                      k.zakres = "28.10–03.11";
                      k.godzinaDanych = "08:08";
                      bl = belkaK2(k);
                    } else {
                      DaneSyp30 k;
                      k.zakres = "16.10–14.11";
                      bl = belkaSyp30(k);
                    }
                    przyp++;
                    PlotnoR p;
                    belka(p, bl);
                    UkladSrodka u = ukladSrodkaBelki(bl);
                    // alarm zawsze, gdy nieaktualne
                    if (nieakt && ekr < 4) {
                      nieaktNocy++;
                      poleZawsze += !bl.alarm.empty();
                    }
                    if (ekr == 4) {  // pole nieaktualnych nie, znak wersji tak
                      przypSyp++;
                      poleSypialni += !bl.alarm.empty();
                      wersjaZnikaSypialni += wer && !znakWersjiNaBelce(bl);
                    }
                    // Portret z polem bez `noc na {dzień} {data}`
                    if (ekr == 1 && nieakt && (u.tekst.find("noc na") != std::string::npos || u.tekst.find(".") != std::string::npos))
                      portretZData++;
                    // szerokość środka (tekst + pole + poPolu + klepsydra) ≤ miejsce (16 px przed prawym polem)
                    int w = szer(F_P22B, u.tekst);
                    bool cos = !u.tekst.empty();
                    if (!bl.alarm.empty()) { w += (cos ? BELKA_ODSTEP_POLA : 0) + szerPolaAlarmu(bl.alarm); cos = true; }
                    if (!bl.poPolu.empty()) { w += (cos ? BELKA_ODSTEP_POLA : 0) + szer(F_P22B, bl.poPolu); cos = true; }
                    if (u.klepsydra) w += (cos ? BELKA_ODSTEP_POLA : 0) + szerKlepsydry(u.slowo);
                    if (xSrodkaBelki(bl) + w > xPrawegoPolaBelki(bl) - BELKA_ODSTEP_PRAWEGO) nakladanie++;
                    // 16 px przed prawym polem w pasie belki bez czerni (piksele)
                    int xp = xPrawegoPolaBelki(bl);
                    if (p.czarne(xp - BELKA_ODSTEP_PRAWEGO, 0, xp, BELKA_LINIA_Y) != 0) nakladanie++;
                    if (wer && !znakWersjiNaBelce(bl)) {
                      wersjaZnika++;
                      if (!kl) wersjaZnikaBezKlepsydry++;
                    }
                    if (kl && !u.klepsydra) klepsZnika++;
                    if (kl && u.klepsydra && !u.slowo) slowoZnika++;
                    // godzina odczytu: gdy była w pełnym środku, ma zostać (R-EKR-65)
                    if (bl.srodek.find("odczyt ") != std::string::npos && u.tekst.find("odczyt ") == std::string::npos)
                      godzinaZnika++;
                  }
  printf("  przypadków %ld; pole przy nieaktualnych: %ld; nakładanie/brak 16 px: %ld; znak wersji ustępuje: %ld "
         "(w tym bez klepsydry %ld); klepsydra ustępuje: %ld; słowo `pobieram` ustępuje: %ld; godzina odczytu znika: %ld\n",
         przyp, poleZawsze, nakladanie, wersjaZnika, wersjaZnikaBezKlepsydry, klepsZnika, slowoZnika, godzinaZnika);
  printf("  pole: %ld z %ld przypadków nieaktualnych na ekranach o nocy; Sypialnia (%ld przypadków): pole %ld, znak "
         "wersji ustąpił %ld; Portret z polem i datą nocy: %ld\n",
         poleZawsze, nieaktNocy, przypSyp, poleSypialni, wersjaZnikaSypialni, portretZData);
  SPRAWDZ(nakladanie == 0);
  SPRAWDZ(godzinaZnika == 0);
  SPRAWDZ(poleZawsze == nieaktNocy && nieaktNocy > 0);
  SPRAWDZ(przypSyp > 0 && poleSypialni == 0 && wersjaZnikaSypialni == 0);
  SPRAWDZ_RB("", portretZData == 0, "Portret z polem `! dane z {data}` i datą `noc na {dzień} {data}` innej nocy");
  ustawZnakiBelki(ZnakiBelki());
  ustawBaterieEkranu(bat(78));
}

// RB: K1 z danymi nieaktualnymi przy kablu: czy ustępuje coś ważniejszego niż znak wersji (data dnia jest pusta,
// więc nic). Sprawdzamy, czy na K1 przy nowej wersji znak znika bez żadnego śladu (stopka też bez ikony).
static void testZnakWersjiK1Znika() {
  ustawBaterieEkranu(bat(60, true));
  ZnakiBelki z;
  z.nowaWersja = true;
  ustawZnakiBelki(z);
  DaneK1 k;
  k.dataNocy = "28.08";
  k.dzienNocy = "śr";
  k.ubiegla = true;
  k.godzinaDanych = "08:08";
  k.romb = true;
  k.ocena = "ocena ograniczona";
  Belka b = belkaK1(k);
  bool naBelce = znakWersjiNaBelce(b);
  {
    IkonaStopki ik(true);  // jak dyrygent przy ekranie dnia; K1 rysowany bez IkonaStopki, tu najgorszy przypadek
    PlotnoR p;
    belka(p, b);
    bool wStopce = ikonaAktualizacjiWlaczona();
    printf("(4) K1 `śr 28.08 · odczyt 08:08 ◆ · ocena ograniczona`, kabel, nowa wersja: znak na belce %d, w stopce %d\n",
           naBelce, wStopce);
    // na najciaśniejszej belce K1 znak
    // nowej wersji ustępuje (pole, romb i ocena nie ustępują), a stopka ekranu z belką nie ma ikony; ten sam stan
    // panelu pokazuje znak na K2, Sypialni i U-01 (i na ekranie dnia), więc informacja nie ginie z panelu
    SPRAWDZ(!naBelce && !wStopce);
  }
  {
    DaneK2 k2;
    k2.zakres = "28.10–03.11";
    k2.godzinaDanych = "08:08";
    DaneSyp30 sy;
    sy.zakres = "16.10–14.11";
    PlotnoR a, b, c;
    belka(a, belkaK2(k2));
    belka(b, belkaSyp30(sy));
    belka(c, Belka{nazwaStronyKokpitu("Ustawienia", "4 / 4"), ""});
    SPRAWDZ(znakWersjiNaBelce(belkaK2(k2)) && znakWersjiNaBelce(belkaSyp30(sy)) &&
            znakWersjiNaBelce(Belka{nazwaStronyKokpitu("Ustawienia", "4 / 4"), ""}));
    printf("(4) ten sam stan na K2, Sypialni i U-01: znak nowej wersji na belce 1\n");
  }
  ustawZnakiBelki(ZnakiBelki());
  ustawBaterieEkranu(bat(78));
}

// ============================================================================================================
// Dyrygent z atrapami
// ============================================================================================================
static const KonfiguracjaPanelu KONF = {"192.168.1.20", "cpap", "haslo", "", "", true};
static const char* STATUS_SPOCZYNEK =
    "{\"version\":\"v2.2.1-25-gebbd145\",\"pna\":\"AirCurve_10_CS_PaceWave\",\"mode\":\"ASVAuto\",\"system\":\"IDLE\","
    "\"rop\":0,\"edf\":\"idle\",\"edf_prefix\":\"\",\"report_state\":\"idle\"}";
static const char* STATUS_TERAPIA = "{\"version\":\"v2.2.1\",\"rop\":1,\"report_state\":\"blocked\"}";
static const char* POLA_V3 =
    "data;wazna;m01_min;ahi;cmed;cp95;wmed;wp95;emed;ep95;psmed;psp95;ustawienia;zrodlo;czesciowa;epap_str;oa;ca;h;csr;"
    "import;m13;m17;pna;srn;m08;mpa;mpi;ipc;eep;mns;mxs;ua;m10;m10p;eai;eax;ans;axs;epr;tryb;m18";

static std::string raport(long day) {
  std::string d = std::to_string(day);
  return "{\"day\":" + d + ",\"present\":true,\"partial\":false,\"used_seconds\":28920,\"ahi\":1.4,"
         "\"events_known\":true,\"session_count\":1,\"counts\":{\"oa\":1,\"ca\":5,\"h\":4},\"channels\":[{\"id\":"
         "\"Press\",\"unit\":\"cmH2O\",\"median\":9.8,\"p95\":13.6,\"source\":\"edf\"},{\"id\":\"Leak\",\"unit\":"
         "\"L/min\",\"median\":2.4,\"p95\":9.6,\"source\":\"edf\"}],\"settings\":{\"available\":true,\"fields\":"
         "[{\"tag\":\"MOP\",\"value\":\"ASVAuto\",\"option\":8}]}}";
}
static std::string lista(long od, long doDnia) {
  std::string l = "{\"source\":\"edf\",\"days\":[";
  for (long x = od; x <= doDnia; x++)
    l += std::string(x > od ? "," : "") + "{\"day\":" + std::to_string(x) + ",\"session_count\":1,\"stale\":false}";
  return l + "]}";
}
static Noc nocH(const std::string& data) {
  Noc n;
  n.data = data;
  n.wazna = true;
  n.m01 = 480;
  n.m18 = 660;
  n.ahi = 1.5f;
  n.tryb = "ASVAuto";
  return n;
}
struct PanelR {
  Panel p;
  explicit PanelR(const std::string& ostatnia, int ile = 5) {
    zerujPrzedSnem(p.start.l, identyfikatorWersji(p.start.napisWersji));
    std::string w;
    for (int i = ile - 1; i >= 0; i--) w += wierszNocyV3(nocH(dodajDni(ostatnia, -i))) + "\n";
    p.pliki.pliki["/dane/noce/2026.csv"] = std::string("#cpap-eink noce 4\n") + POLA_V3 + "\n" + w + "#koniec " +
                                           std::to_string(ile) + "\n";
    p.pliki.pliki["/konfiguracja.txt"] = "noc_zrodlo=json\n";
  }
};
struct WybR {
  uint32_t ep = 0;
  int minuta = 0;
  int odswiezen = 0, polaczen = 0;
  bool pole = false, klepsydra = false, przerwane = false, spal = false;
  bool klepsydraNaKoniec = false;  // ostatnie odświeżenie e-ink w tym wybudzeniu pokazało klepsydrę
  std::vector<std::string> log;
};
static bool zawiera(const std::string& s, const char* f) { return s.find(f) != std::string::npos; }
static WybR wybudz(PanelR& s, uint32_t t, PowodWybudzenia pw) {
  s.p.zegar.epoch = t;
  s.p.zegar.msTeraz = 0;
  s.p.start.przyczynaStartu = pw == PowodWybudzenia::START ? PrzyczynaStartu::ZASILANIE : PrzyczynaStartu::USPIENIE;
  s.p.start.powodWybudzenia = pw;
  s.p.siec.polaczonaTeraz = false;
  s.p.d.z.clear();
  int snow = s.p.sen.snow;
  Dyrygent(s.p.adaptery(), KONF).wybudzenie();
  WybR w;
  w.ep = t;
  w.minuta = minutaLok(t);
  w.spal = s.p.sen.snow == snow + 1;
  bool kl = false;  // klepsydra w klatce, która czeka na odświeżenie
  for (auto& z : s.p.d.z) {
    w.log.push_back(z);
    w.polaczen += z == "siec.polacz";
    if (z == "ekran.odswiez") {
      w.odswiezen++;
      w.klepsydraNaKoniec = kl;
      kl = false;
    }
    if (zawiera(z, "belka: dane nieaktualne")) w.pole = true;
    if (zawiera(z, "belka: klepsydra")) w.klepsydra = kl = true;
    if (zawiera(z, "bez odświeżenia e-ink")) kl = false;
    if (zawiera(z, "przerwane przyciskiem")) w.przerwane = true;
  }
  return w;
}
static void drukujLog(const WybR& w, const char* filtr = nullptr) {
  for (auto& l : w.log)
    if (!filtr || l.find(filtr) != std::string::npos) printf("      | %s\n", l.c_str());
}
using Odpowiedzi = std::function<void(PanelR&, uint32_t)>;
static std::vector<WybR> symuluj(PanelR& s, const Odpowiedzi& odp, uint32_t od, uint32_t doT) {
  std::vector<WybR> v;
  uint32_t t = od;
  PowodWybudzenia pw = PowodWybudzenia::START;
  for (int i = 0; i < 1200 && t < doT; i++) {
    s.p.siec.odpowiedzi.clear();
    odp(s, t);
    WybR w = wybudz(s, t, pw);
    v.push_back(w);
    if (!w.spal) break;
    uint32_t snu = s.p.sen.plan.budzZegarem ? s.p.sen.plan.sekund : OKRES_SNU_S;
    t += s.p.zegar.msTeraz / 1000 + (snu ? snu : 1);
    pw = PowodWybudzenia::ZEGAR;
  }
  return v;
}
// AirBridge z nocą z każdego poranka od `od` do dnia chwili t (noc z poranka D dostępna od D 07:05)
static Odpowiedzi zdrowyAB(long pierwszaDoba) {
  return [pierwszaDoba](PanelR& s, uint32_t t) {
    std::string d = dataLok(t);
    long ost = nrDnia(d) - 1;                              // doba nocy z poranka d
    if (minutaLok(t) < 7 * 60 + 5) ost--;                  // przed 07:05 jeszcze bez nocy z dziś
    s.p.siec.dodaj("/api/status", 200, STATUS_SPOCZYNEK);
    s.p.siec.dodaj(sciezkaListy(), 200, lista(pierwszaDoba, ost));
    for (long x = pierwszaDoba; x <= ost; x++) s.p.siec.dodaj(sciezkaNocy(x), 200, raport(x));
  };
}
static int ilePole(const std::vector<WybR>& v, uint32_t od, uint32_t doT) {
  int n = 0;
  for (auto& w : v)
    if (w.ep >= od && w.ep < doT) n += w.pole;
  return n;
}
static int ileWybudzen(const std::vector<WybR>& v, uint32_t od, uint32_t doT) {
  int n = 0;
  for (auto& w : v) n += w.ep >= od && w.ep < doT;
  return n;
}

// D1. Zdrowy użytkownik, 3 doby: noc co rano o 07:05, AirBridge zawsze odpowiada → pole nigdy.
static void testZdrowyTrzyDoby() {
  PanelR s("2026-11-03");
  std::vector<WybR> v = symuluj(s, zdrowyAB(nrDnia("2026-11-03") - 5), ep(2026, 11, 4, 4, 0), ep(2026, 11, 7, 4, 0));
  int pole = ilePole(v, 0, ~0u);
  printf("(1) zdrowy użytkownik 4.11–7.11: wybudzeń %d, z polem %d\n", (int)v.size(), pole);
  SPRAWDZ(pole == 0);
}

// D2. Noc bez terapii 4.11 (AirBridge odpowiada, nocy nie ma), 5.11 noc jest → pole nigdy (C-2: tekst `dane z`)
static void testNocBezTerapiiDwieDoby() {
  PanelR s("2026-11-03");
  long d0 = nrDnia("2026-11-03") - 5;
  Odpowiedzi o = [d0](PanelR& p, uint32_t t) {
    std::string d = dataLok(t);
    long ost = nrDnia(d) - 1;
    if (minutaLok(t) < 7 * 60 + 5) ost--;
    p.p.siec.dodaj("/api/status", 200, STATUS_SPOCZYNEK);
    std::string l = "{\"source\":\"edf\",\"days\":[";
    bool pierw = true;
    for (long x = d0; x <= ost; x++) {
      if (x == nrDnia("2026-11-03")) continue;  // noc z poranka 4.11: bez terapii
      l += std::string(pierw ? "" : ",") + "{\"day\":" + std::to_string(x) + ",\"session_count\":1,\"stale\":false}";
      pierw = false;
    }
    p.p.siec.dodaj(sciezkaListy(), 200, l + "]}");
    for (long x = d0; x <= ost; x++) p.p.siec.dodaj(sciezkaNocy(x), 200, raport(x));
  };
  std::vector<WybR> v = symuluj(s, o, ep(2026, 11, 4, 4, 0), ep(2026, 11, 5, 12, 0));
  printf("(1) noc bez terapii 4.11, AirBridge odpowiada: z polem 4.11 %d z %d, 5.11 do 07:05 %d z %d\n",
         ilePole(v, ep(2026, 11, 4, 0, 0), ep(2026, 11, 5, 0, 0)), ileWybudzen(v, ep(2026, 11, 4, 0, 0), ep(2026, 11, 5, 0, 0)),
         ilePole(v, ep(2026, 11, 5, 0, 0), ep(2026, 11, 5, 7, 5)), ileWybudzen(v, ep(2026, 11, 5, 0, 0), ep(2026, 11, 5, 7, 5)));
  SPRAWDZ(ilePole(v, 0, ~0u) == 0);
}

// D3…D6. Po 11:00 AirBridge odpowiada na `/api/status`, ale noc nie przychodzi z innego powodu (lista urwana,
// lista nieczytelna E-03, raport nocy nieudany, logowanie odrzucone). Kryterium właściciela: „brak ostatniej nocy po
// zamknięciu okna porannego” → pole; spec R-EKR-67 fail-path: E-03 i odrzucone logowanie → pole.
static void scenariuszAwariiPo11(const char* opis, const char* id, int rodzaj) {
  PanelR s("2026-11-03");
  long ost = nrDnia("2026-11-03");  // noc z poranka 4.11 na AirBridge
  Odpowiedzi o = [ost, rodzaj](PanelR& p, uint32_t t) {
    if (minutaLok(t) < 4 * 60 + 30 && dataLok(t) == "2026-11-04") return;  // start
    if (rodzaj == 3) {  // odrzucone logowanie
      p.p.siec.dodaj("/api/status", 401, "");
      return;
    }
    p.p.siec.dodaj("/api/status", 200, STATUS_SPOCZYNEK);
    if (rodzaj == 0) p.p.siec.dodaj(sciezkaListy(), 500, "blad");                 // lista: błąd serwera
    if (rodzaj == 1) p.p.siec.dodaj(sciezkaListy(), 200, "{\"days\":[{\"da");     // lista nieczytelna (E-03)
    if (rodzaj == 2) {                                                             // noc na liście, raport się nie udaje
      p.p.siec.dodaj(sciezkaListy(), 200, lista(ost - 5, ost));
      p.p.siec.dodaj(sciezkaNocy(ost), 500, "blad");
      for (long x = ost - 5; x < ost; x++) p.p.siec.dodaj(sciezkaNocy(x), 200, raport(x));
    }
  };
  std::vector<WybR> v = symuluj(s, o, ep(2026, 11, 4, 4, 0), ep(2026, 11, 4, 18, 0));
  int po11 = ileWybudzen(v, ep(2026, 11, 4, 11, 0), ~0u), polePo11 = ilePole(v, ep(2026, 11, 4, 11, 0), ~0u);
  printf("(1) %s: od 11:00 wybudzeń %d, z polem %d\n", opis, po11, polePo11);
  // co mówi treść ekranu dnia o 12:00 (pierwsze wybudzenie po 12:00)
  for (auto& w : v)
    if (w.ep >= ep(2026, 11, 4, 11, 0)) {
      drukujLog(w, "ekran ");
      drukujLog(w, "E-0");
      drukujLog(w, "E-10");
      break;
    }
  SPRAWDZ_RB(id, po11 == 14 && polePo11 == po11, opis);  // pole w każdym wybudzeniu od 11:00
}

// panel wyłączony (bez zasilania) od 10.11 do 14.11 12:00; AirBridge ma noce z poranków 11.11…13.11,
// noc 14.11 bez terapii (brak na liście). (a) raporty zaległych dób się nie udają (500): stan nocy oczekiwanej
// nieznany, pole w każdym wybudzeniu do 18:00; (b) raporty się udają (najnowsze najpierw, przed rysowaniem): lista bez
// zaległych dób nowszych od ostatniej zapisanej nocy → stan znany, bez pola.
static void testWylaczonyTrzyDni() {
  const long d10 = nrDnia("2026-11-10") - 1, d13 = nrDnia("2026-11-13") - 1;
  for (int wariant = 0; wariant < 2; wariant++) {
    PanelR s("2026-11-10");
    Odpowiedzi o = [&](PanelR& p, uint32_t) {
      p.p.siec.dodaj("/api/status", 200, STATUS_SPOCZYNEK);
      p.p.siec.dodaj(sciezkaListy(), 200, lista(d10 - 4, d13));
      for (long x = d10 - 4; x <= d13; x++) {
        if (x > d10 && wariant == 0) p.p.siec.dodaj(sciezkaNocy(x), 500, "blad");
        else p.p.siec.dodaj(sciezkaNocy(x), 200, raport(x));
      }
    };
    std::vector<WybR> v = symuluj(s, o, ep(2026, 11, 14, 12, 0), ep(2026, 11, 14, 18, 0));
    int n = (int)v.size(), pole = ilePole(v, 0, ~0u);
    bool pierwsze = !v.empty() && v[0].pole;
    int poleDalej = ilePole(v, v.size() > 1 ? v[1].ep : ~0u, ~0u);
    printf("(1) panel wyłączony 10.11–14.11 12:00, raporty zaległych %s: wybudzeń %d, z polem %d (pierwsze %d)\n",
           wariant == 0 ? "nieudane" : "udane", n, pole, pierwsze);
    SPRAWDZ(n > 3);
    if (wariant == 0) SPRAWDZ_RB("", pierwsze && pole == n, "zaległe niepobrane doby po kilku dniach wyłączenia: bez pola");
    else SPRAWDZ(pole == 0 && poleDalej == 0);  // zaległe pobrane przed rysowaniem, noc 14.11 bez terapii: stan znany
  }
}

// Poznanie stanu nocy bez nocy na liście: (a) lista pusta po 11:00 → noc bez terapii, bez pola; (b) terapia trwa od
// 06:00 do wieczora (status `blocked`, lista nieczytana), noc z wczoraj zapisana → noc z dziś w budowie, bez pola;
// (c) jak (b), ale noc z wczoraj nieznana (nie ma jej w historii, listy nie było) → pole od 11:00
static void testPoznanieBezListyNocy() {
  for (int w = 0; w < 3; w++) {
    PanelR s(w == 2 ? "2026-11-02" : "2026-11-03");
    Odpowiedzi o = [w](PanelR& p, uint32_t t) {
      if (w == 0) {
        p.p.siec.dodaj("/api/status", 200, STATUS_SPOCZYNEK);
        p.p.siec.dodaj(sciezkaListy(), 200, "{\"source\":\"edf\",\"days\":[]}");
      } else {
        p.p.siec.dodaj("/api/status", 200, minutaLok(t) >= 6 * 60 ? STATUS_TERAPIA : STATUS_SPOCZYNEK);
      }
    };
    std::vector<WybR> v = symuluj(s, o, ep(2026, 11, 4, 4, 0), ep(2026, 11, 4, 18, 0));
    uint32_t od11 = ep(2026, 11, 4, 11, 0);
    int n = ileWybudzen(v, od11, ~0u), pole = ilePole(v, od11, ~0u);
    const char* opis[] = {"lista pusta", "terapia trwa, noc z wczoraj zapisana", "terapia trwa, noc z wczoraj nieznana"};
    printf("(1) %s: od 11:00 wybudzeń %d, z polem %d\n", opis[w], n, pole);
    SPRAWDZ(n > 0);
    if (w < 2) SPRAWDZ(pole == 0);
    else SPRAWDZ(pole == n);
  }
}

// w dyrygencie: zaległe doby w toku (panel wyłączony 10 dni, po ZALEGLE_NA_WYBUDZENIE dób na wybudzenie);
// U-05 przy znanym zegarze `{DD.MM hh:mm} · pobieram` bez usterki; to samo przy nieznanym zegarze: bez `pobieram`
// (bez oceny, ADR-0029 pkt 7)
static std::string pobranieNaU05(PanelR& s, uint32_t t, bool zegarZnany) {
  s.p.start.budzace = 1u << (int)Przycisk::PRAWY;
  s.p.start.stan.ekran = (uint8_t)Ekran::FORMULARZ;
  s.p.przyciski.skrypt = {{0, (uint8_t)(1u << (int)Przycisk::PRAWY)}, {2300, 0}};  // U-05 z U-03 przez ▶ 2 s
  s.p.zegar.epoch = zegarZnany ? t : 0;
  s.p.zegar.msTeraz = 0;
  s.p.siec.ntpJest = zegarZnany;
  s.p.start.przyczynaStartu = PrzyczynaStartu::USPIENIE;
  s.p.start.powodWybudzenia = PowodWybudzenia::PRZYCISK;
  s.p.siec.polaczonaTeraz = false;
  s.p.siec.odpowiedzi.clear();
  s.p.d.z.clear();
  Dyrygent dy(s.p.adaptery(), KONF);
  dy.wybudzenie();
  for (auto& l : dy.teksty().linieTestu)
    if (l.rfind("Ostatnie pobranie: ", 0) == 0) return l;
  return "";
}
static void testU05ZaleglePobieram() {
  const long d3 = nrDnia("2026-11-03") - 1, d13 = nrDnia("2026-11-13") - 1;
  PanelR s("2026-11-03");
  Odpowiedzi o = [&](PanelR& p, uint32_t) {
    p.p.siec.dodaj("/api/status", 200, STATUS_SPOCZYNEK);
    p.p.siec.dodaj(sciezkaListy(), 200, lista(d3 - 4, d13));
    for (long x = d3 - 4; x <= d13; x++) p.p.siec.dodaj(sciezkaNocy(x), 200, raport(x));
  };
  std::vector<WybR> v = symuluj(s, o, ep(2026, 11, 14, 12, 0), ep(2026, 11, 14, 12, 1));
  if (getenv("LOG_U05") && !v.empty()) drukujLog(v[0]);
  printf("(6) po pierwszym wybudzeniu: zaległe %d, czekające %d, pole %d\n", s.p.start.stan.ab.zalegle,
         s.p.start.stan.ab.czekajace, !v.empty() && v[0].pole);
  SPRAWDZ(s.p.start.stan.ab.zalegle > 0 && s.p.start.stan.ab.czekajace == 0);
  // najnowsze noce pobrane najpierw (13.11 jest), starsza historia w toku nie czyni danych na ekranie nieaktualnymi
  SPRAWDZ(!v.empty() && !v[0].pole);
  PanelR z = s;
  std::string a = pobranieNaU05(s, ep(2026, 11, 14, 12, 5), true);
  std::string b = pobranieNaU05(z, ep(2026, 11, 14, 12, 5), false);
  printf("(6) U-05 przy zaległych dobach: zegar znany `%s`, zegar nieznany `%s`\n", a.c_str(), b.c_str());
  SPRAWDZ(a.find("pobieram") != std::string::npos);
  SPRAWDZ_RB("", !b.empty() && b.find("pobieram") == std::string::npos,
          "U-05 przy nieznanym zegarze pokazuje `pobieram` (wygląda jak sprawny)");
}

// D7. Klepsydra: ◀ bez akcji na ekranie dnia o 06:10, terapia trwa (bez nowej nocy): ile odświeżeń e-ink
static void testKlepsydraKosztOdswiezen() {
  PanelR s("2026-11-03");
  Odpowiedzi terapia = [](PanelR& p, uint32_t) { p.p.siec.dodaj("/api/status", 200, STATUS_TERAPIA); };
  symuluj(s, terapia, ep(2026, 11, 4, 4, 0), ep(2026, 11, 4, 6, 0));
  auto przycisk = [&](uint32_t t, uint8_t bit, Ekran e) {
    s.p.start.budzace = bit;
    s.p.start.stan.ekran = (uint8_t)e;
    s.p.start.stan.ekranOd = t - 600;
    s.p.przyciski.skrypt = {{0, bit}, {150, 0}};
    s.p.siec.odpowiedzi.clear();
    terapia(s, t);
    return wybudz(s, t, PowodWybudzenia::PRZYCISK);
  };
  const uint8_t L = 1u << (int)Przycisk::LEWY, P = 1u << (int)Przycisk::PRAWY;
  WybR a = przycisk(ep(2026, 11, 4, 6, 10, 30), L, Ekran::DZIEN);
  printf("(3) ◀ bez akcji, bez nowej nocy: odświeżeń e-ink %d (przed zmianą 0), klepsydra %d, klepsydra na końcu %d\n",
         a.odswiezen, a.klepsydra, a.klepsydraNaKoniec);
  SPRAWDZ(!a.klepsydraNaKoniec);
  // liczba odświeżeń jak przed częścią C (0, 0, 1), klepsydra w żadnej klatce
  SPRAWDZ_RB("", a.odswiezen == 0 && !a.klepsydra,
          "◀ bez akcji rano bez nocy: odświeżenie e-ink tylko dla klepsydry (przed częścią C: 0)");
  // poranek z 6 naciśnięciami ◀ co 2 min 05 s (każde z pobraniem): suma odświeżeń
  int suma = 0;
  for (int i = 1; i <= 6; i++) suma += przycisk(ep(2026, 11, 4, 6, 10, 30) + (uint32_t)(i * 125), L, Ekran::DZIEN).odswiezen;
  printf("(3) 6 kolejnych ◀ co 125 s: odświeżeń %d (przed zmianą 0)\n", suma);
  SPRAWDZ_RB("", suma == 0, "6 naciśnięć ◀ co 125 s: odświeżenia e-ink tylko dla klepsydry (przed częścią C: 0)");
  // ▶ → K1 i z powrotem: każde przejście 2 odświeżenia
  WybR b = przycisk(ep(2026, 11, 4, 7, 0, 0), P, Ekran::DZIEN);
  printf("(3) ▶ → K1 bez nowej nocy: odświeżeń %d (przed zmianą 1)\n", b.odswiezen);
  SPRAWDZ(!b.klepsydraNaKoniec);
  SPRAWDZ_RB("", b.odswiezen == 1 && !b.klepsydra, "▶ → K1 bez nowej nocy: odświeżenie więcej dla klepsydry");
}

// D8. Klepsydra przy pobraniu przerwanym naciśnięciem / zerwanym połączeniu / timeout: czy zostaje na e-inku
static void testKlepsydraPrzerwane() {
  const uint8_t L = 1u << (int)Przycisk::LEWY, S = 1u << (int)Przycisk::SRODEK, P = 1u << (int)Przycisk::PRAWY;
  struct Wariant {
    const char* opis;
    uint8_t budzacy, przerywajacy;
    bool kolejka;  // naciśnięcie trafia do kolejki ISR (sprzęt) czy tylko odczyt stanu
  };
  std::vector<Wariant> w = {{"◀ budzi, ◀ przerywa, w kolejce", L, L, true},
                            {"◀ budzi, ◀ przerywa, poza kolejką", L, L, false},
                            {"◀ budzi, ↻ przerywa, w kolejce", L, S, true},
                            {"▶ budzi (K1), ◀ przerywa, w kolejce", P, L, true},
                            {"◀ budzi, ↻ przytrzymane 6 s (2,5 s) przerywa", L, S, true}};
  int zostaje = 0, przerwanych = 0;
  for (auto& x : w) {
    for (uint32_t tPrzerwania = 1500; tPrzerwania <= 30000; tPrzerwania += 1000) {
      PanelR s("2026-11-03");
      uint32_t t = ep(2026, 11, 4, 6, 10, 30);
      s.p.start.budzace = x.budzacy;
      s.p.start.stan.ekran = (uint8_t)Ekran::DZIEN;
      s.p.start.stan.ekranOd = t - 600;
      uint32_t trzym = std::string(x.opis).find("2,5") != std::string::npos ? 2600 : 300;
      s.p.przyciski.skrypt = {{0, x.budzacy}, {150, 0}, {tPrzerwania, x.przerywajacy}, {tPrzerwania + trzym, 0}};
      if (x.kolejka) s.p.przyciski.kolejka = {{(int)(x.przerywajacy == L ? Przycisk::LEWY : x.przerywajacy == S ? Przycisk::SRODEK : Przycisk::PRAWY), trzym}};
      s.p.siec.odpowiedzi.clear();
      s.p.siec.dodaj("/api/status", 200, STATUS_SPOCZYNEK);
      s.p.siec.dodaj(sciezkaListy(), 200, lista(nrDnia("2026-11-03") - 5, nrDnia("2026-11-03")));
      s.p.siec.dodaj("/api/report", 202, "{\"job\":\"j1\",\"pending\":true}");  // raport i odpytywanie joba bez końca
      WybR r = wybudz(s, t, PowodWybudzenia::PRZYCISK);
      if (getenv("LOG_REC1") && tPrzerwania == 5500 && x.budzacy == L && x.przerywajacy == L && !x.kolejka) drukujLog(r);  // przebieg do wglądu
      if (!r.przerwane) continue;
      przerwanych++;
      if (r.klepsydraNaKoniec) {
        zostaje++;
        printf("  klepsydra ZOSTAJE: %s, przerwanie w %u ms, odświeżeń %d\n", x.opis, tPrzerwania, r.odswiezen);
        if (getenv("LOG_REC")) drukujLog(r);
      }
    }
  }
  printf("(3) pobranie przerwane naciśnięciem: przerwanych przebiegów %d, z klepsydrą na końcu %d\n", przerwanych, zostaje);
  SPRAWDZ_RB("RB-klepsydra-przerwana", zostaje == 0, "klepsydra zostaje na e-inku po pobraniu przerwanym naciśnięciem");
  // zerwane połączenie / brak sieci / timeout: bez przerwania przyciskiem
  for (int rodzaj = 0; rodzaj < 3; rodzaj++) {
    PanelR s("2026-11-03");
    uint32_t t = ep(2026, 11, 4, 6, 10, 30);
    s.p.start.budzace = L;
    s.p.start.stan.ekran = (uint8_t)Ekran::DZIEN;
    s.p.start.stan.ekranOd = t - 600;
    s.p.przyciski.skrypt = {{0, L}, {150, 0}};
    s.p.siec.odpowiedzi.clear();
    if (rodzaj == 0) s.p.siec.siecJest = false;
    if (rodzaj == 1) s.p.siec.dodaj("/api/status", 0, "", 20000);  // brak odpowiedzi po 20 s
    if (rodzaj == 2) {
      s.p.siec.dodaj("/api/status", 200, STATUS_SPOCZYNEK);
      s.p.siec.dodaj(sciezkaListy(), 200, lista(nrDnia("2026-11-03") - 5, nrDnia("2026-11-03")));
      s.p.siec.dodaj(sciezkaNocy(nrDnia("2026-11-03")), 202, "{\"job\":\"j1\",\"pending\":true}", 3000);  // job bez końca
    }
    WybR r = wybudz(s, t, PowodWybudzenia::PRZYCISK);
    printf("(3) pobranie nieudane (%s): klepsydra %d, na końcu %d, odświeżeń %d\n",
           rodzaj == 0 ? "brak Wi-Fi" : rodzaj == 1 ? "AirBridge milczy" : "job raportu bez końca", r.klepsydra,
           r.klepsydraNaKoniec, r.odswiezen);
    SPRAWDZ(!r.klepsydraNaKoniec);
  }
}

// D9. Znak nowej wersji na ekranie L z pilnością A albo B (R-EKR-69: bez znaku). Jak test autora
// testR_KONF_45_sprawaABNaEkranie (15 nocy z AHI 12 → L-03, pilność B).
static void testZnakWersjiNaL() {
  sprawy::zrodloTestowe = sprawy::ZrodloWynikow();
  Panel p;
  const int NOCY = 15;
  long ost = nrDnia("2026-11-04") - 1;
  std::string l = "{\"source\":\"edf\",\"days\":[";
  for (long x = ost - NOCY + 1; x <= ost; x++)
    l += std::string(x > ost - NOCY + 1 ? "," : "") + "{\"day\":" + std::to_string(x) + ",\"session_count\":1,\"stale\":false}";
  l += "]}";
  auto odpowiedzi = [&]() {
    p.siec.odpowiedzi.clear();
    p.siec.dodaj("/api/status", 200, STATUS_SPOCZYNEK);
    p.siec.dodaj(sciezkaListy(), 200, l);
    for (long x = ost - NOCY + 1; x <= ost; x++) {
      std::string r = raport(x);
      size_t q = r.find("\"ahi\":1.4");
      p.siec.dodaj(sciezkaNocy(x), 200, r.replace(q, 9, "\"ahi\":12.0"));
    }
  };
  auto zegarem = [&](uint32_t t, PowodWybudzenia pw, PrzyczynaStartu ps) {
    p.zegar.epoch = t;
    p.zegar.msTeraz = 0;
    p.start.przyczynaStartu = ps;
    p.start.powodWybudzenia = pw;
    p.przyciski.skrypt.clear();
    p.siec.polaczonaTeraz = false;
    p.d.z.clear();
    p.port.log.clear();
    zerujPrzedSnem(p.start.l, identyfikatorWersji(p.start.napisWersji));
  };
  auto wLogu = [&](const char* f) {
    for (auto& x : p.port.log)
      if (x.find(f) != std::string::npos) return true;
    return false;
  };
  bool jest = false;
  for (int h = 0; h < 12 && !jest; h++) {
    odpowiedzi();
    zegarem(ep(2026, 11, 4, 7, 0) + (uint32_t)(h * 1800), PowodWybudzenia::START, PrzyczynaStartu::ZASILANIE);
    p.start.stan = StanRtc();
    Dyrygent(p.adaptery(), KONF).wybudzenie();
    jest = wLogu("ekran dnia: wzorzec L-03");
  }
  SPRAWDZ(jest);
  sekrety::zapiszKluczWydan(p.nvs, "github_pat_ZNACZNIK_0123456789abcdef");
  aktualizacja::Stan st = aktualizacja::wczytaj(p.nvs);
  st.znana = "9.9.9";
  aktualizacja::zapisz(p.nvs, st);
  odpowiedzi();
  zegarem(ep(2026, 11, 4, 13, 0), PowodWybudzenia::ZEGAR, PrzyczynaStartu::USPIENIE);
  Dyrygent(p.adaptery(), KONF).wybudzenie();
  bool l03 = wLogu("wzorzec L-03"), znak = wLogu("belka: znak nowej wersji"), ikona = wLogu("ikona aktualizacji");
  printf("(4) ekran L-03 (pilność B), znana nowsza wersja: L-03 %d, znak na belce %d, ikona w stopce %d\n", l03, znak, ikona);
  SPRAWDZ(l03 && !znak && !ikona);
}

// D10. U-05 `pobieram`: wiersz przy nieznanym zegarze i bez historii pobrań (ADR-0029 pkt 7)
static void testU05Pobieram() {
  WierszTestu a = wierszU05Pobrania("–", false, false, true, true);  // bezOceny + pobieram
  printf("(6) U-05 `Ostatnie pobranie` bezOceny i pobieram: `%s`, nieSprawdzono %d, usterka %d\n", a.wartosc.c_str(),
         a.nieSprawdzono, a.usterka);
  SPRAWDZ_RB("", a.nieSprawdzono && a.wartosc.find("pobieram") == std::string::npos,
          "`pobieram` ma pierwszeństwo przed „bez oceny” (zegar nieznany, brak kontaktu): wiersz wygląda jak sprawny "
          "(ADR-0029 pkt 7)");
  WierszTestu b = wierszU05Pobrania("12.11 07:05", false, false, false, true);
  SPRAWDZ(!b.usterka && b.wartosc.find("pobieram") != std::string::npos);
  WierszTestu c = wierszU05Pobrania("12.11 07:05", false, true, false, true);  // kontakt stary ma pierwszeństwo
  SPRAWDZ(c.usterka);
}

int main() {
  setenv("TZ", STREFA, 1);
  tzset();
  testTabelaDefinicji();
  testKombinacjeBelki();
  testZnakWersjiK1Znika();
  testU05Pobieram();
  testZdrowyTrzyDoby();
  testNocBezTerapiiDwieDoby();
  scenariuszAwariiPo11("lista nocy urwana (HTTP 500) od rana, status OK", " (lista 500)", 0);
  scenariuszAwariiPo11("lista nocy nieczytelna (E-03), status OK", " (E-03)", 1);
  scenariuszAwariiPo11("noc na liście, raport nocy nieudany (500), status OK", " (raport 500)", 2);
  scenariuszAwariiPo11("odrzucone logowanie (401)", "R-EKR-67 (401)", 3);
  testWylaczonyTrzyDni();
  testPoznanieBezListyNocy();
  testU05ZaleglePobieram();
  testKlepsydraKosztOdswiezen();
  testKlepsydraPrzerwane();
  testZnakWersjiNaL();
  printf("testy_belka_znaki_brzegowe: %d ok, %d błędów (usterki jako sprawdzenia)\n", ok, zle);
  return zle ? 1 : 0;
}
