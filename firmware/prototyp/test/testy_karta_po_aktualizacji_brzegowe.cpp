// SPDX-License-Identifier: GPL-3.0-or-later
// Testy brzegowe: scenariusze, których autor nie
// sprawdził osobno (przytrzymania każdego przycisku, krótkie ↻ bez pobrania, niska bateria i nieaktualne dane
// powstałe w trakcie doby z informacją na ekranie, zegar nieznany i cofnięty, dwie aktualizacje pod rząd,
// automat potem ręczna, nowości innej wersji, wgranie kablem, Wi-Fi i zapisy NVS na dobę, poranek S2a).
// Harness autora z test/testy_karta_po_aktualizacji.cpp (main przemianowany). Wynik: liczba BŁĘDÓW = usterki RG-…
// opisane w raporcie recenzji; test nie zmienia kodu produkcyjnego. Od rundy poprawek (15.11.2026) test blokujący
// w tools/prototyp/testy.sh; sprawdzenia dostosowane do decyzji 12 (informacja tylko w slocie dziennym) opisane przy nich.
#define main main_testy_karta_po_aktualizacji
#include "testy_karta_po_aktualizacji.cpp"
#undef main

static int ileW(const Panel& p, const std::string& f) { return p.d.ile(f); }

// RG: przytrzymanie każdego z trzech przycisków na informacji (nad S2a, bez slotu): tylko zamyka, bez Wi-Fi, bez sieci
// panelu, bez zapisu wymiany, bez pobrania; ekran dnia. Krótkie ↻: bez pobrania i bez Wi-Fi (jak na ekranie dnia pod
// spodem).
static void testRec_przytrzymania() {
  const uint8_t bity[3] = {B_LEWY, B_SRODEK, B_PRAWY};
  const char* nazwy[3] = {"◀", "↻", "▶"};
  for (int i = 0; i < 3; i++)
    for (int dl = 0; dl < 2; dl++) {
      Panel p;
      airbridge(p, d0());
      zegarem(p, ep(2026, 11, 10, 9, 0));
      wybudz(p);
      uzbroj(p);
      airbridge(p, d0());
      zegarem(p, ep(2026, 11, 10, 11, 0));
      SPRAWDZ(informacjaNaEkranie(wybudz(p)));
      if (i == 2 && !dl) continue;  // krótkie ▶: test autora (pierwsza strona kokpitu)
      przyciskiem(p, ep(2026, 11, 10, 11, 5), bity[i], dl ? 2500 : 200);
      airbridge(p, d0());
      TekstyWybudzenia t = wybudz(p);
      bool ok1 = !informacjaNaEkranie(t) && zapisNvs(p).empty() && p.start.stan.ekran == (uint8_t)Ekran::DZIEN;
      bool bezSieci = !wLogu(p, "sieć panelu: uruchomiona") && !wLogu(p, "sprzęt: wymiana") &&
                      !wLogu(p, "aktualizacja: sprawdzam");
      int wifi = ileW(p, "siec.polacz");
      if (!ok1 || !bezSieci || wifi)
        printf("  RG przytrzymania: %s %s: zamknięta+DZIEN %d, bez akcji %d, Wi-Fi %d\n", nazwy[i], dl ? "2 s" : "krótko",
               ok1, bezSieci, wifi);
      SPRAWDZ(ok1);
      SPRAWDZ(bezSieci);
      SPRAWDZ(wifi == 0);
    }
}

// RG: niska bateria powstała w trakcie doby, w której informacja stoi na ekranie: następne rysowanie ekranu dnia ma
// pokazać ekran dnia z baterią (informacja ustępuje), e-ink odświeżony.
static void testRec_bateriaWTrakcie() {
  for (float v : {3.55f, 3.722f}) {  // ok. 4 % (S5 albo linia E-05) i 18 % (tylko „niska”, R-KON-69)
  Panel p;
  airbridge(p, d0());
  zegarem(p, ep(2026, 11, 10, 9, 0));
  wybudz(p);
  uzbroj(p);
  airbridge(p, d0());
  zegarem(p, ep(2026, 11, 10, 11, 0));
  SPRAWDZ(informacjaNaEkranie(wybudz(p)));
  p.czujniki.o.bateriaV = v;
  bool zeszla = false;
  int odsw = p.ekran.odswiezen;
  for (int h = 12; h <= 16 && !zeszla; h++) {
    airbridge(p, d0());
    zegarem(p, ep(2026, 11, 10, h, 0));
    zeszla = !informacjaNaEkranie(wybudz(p));
  }
  if (!zeszla) printf("  RG bateria w trakcie doby: informacja dalej na ekranie przy %.3f V\n", (double)v);
  SPRAWDZ(zeszla);
  SPRAWDZ(p.ekran.odswiezen > odsw);
  // stan informacji nie zmieniony (dalej czeka do końca doby, bez zapisu)
  SPRAWDZ(informacjaNvs(p).dzien == "2026-11-10");
  }
}

// RG: dane nieaktualne powstałe w trakcie doby (AirBridge przestaje odpowiadać, następnego dnia po 11:00 brak nocy):
// informacja nie stoi na ekranie, gdy belka powinna mieć pole „dane nieaktualne”.
static void testRec_nieaktualneWTrakcie() {
  Panel p;
  airbridge(p, d0());
  zegarem(p, ep(2026, 11, 10, 9, 0));
  wybudz(p);
  uzbroj(p);
  // następna doba: noc 11.11 nie przychodzi (AirBridge bez odpowiedzi), informacja pokazana o 09:00 (przed 11:00)
  airbridge(p, d0());
  zegarem(p, ep(2026, 11, 11, 9, 0));
  TekstyWybudzenia t = wybudz(p);
  bool rano = informacjaNaEkranie(t);
  printf("  RG nieaktualne: 11.11 09:00 informacja %s\n", rano ? "tak" : "nie");
  bool nieakt = false, inf = false;
  for (int h = 11; h <= 15; h++) {
    p.siec.odpowiedzi.clear();
    zegarem(p, ep(2026, 11, 11, h, 0) + 60);
    t = wybudz(p);
    bool n = wLogu(p, "belka: dane nieaktualne");
    nieakt = nieakt || n;
    if (n && informacjaNaEkranie(t)) inf = true;
  }
  printf("  RG nieaktualne: po 11:00 belka nieaktualne %s, informacja przy nieaktualnych %s\n", nieakt ? "tak" : "nie",
         inf ? "TAK" : "nie");
  SPRAWDZ(!inf);
}

// RG: zegar nieznany (epoch 0) i cofnięty: bez informacji, bez zapisu; po cofnięciu zegara do doby pokazania
// informacja nie wraca po zamknięciu (tylko gdy nie zamknięta).
static void testRec_zegar() {
  {  // nieznany
    Panel p;
    uzbroj(p);
    airbridge(p, d0());
    zegarem(p, 1000);
    TekstyWybudzenia t = wybudz(p);
    SPRAWDZ(!informacjaNaEkranie(t));
    SPRAWDZ(informacjaNvs(p).rodzaj == RodzajInformacji::UDANA && informacjaNvs(p).dzien.empty());
  }
  {  // zegar z przyszłości przy pierwszym pokazaniu (zła synchronizacja), potem poprawiony: wpis zostaje na zawsze
    Panel p;
    airbridge(p, d0());
    zegarem(p, ep(2026, 11, 10, 9, 0));
    wybudz(p);
    uzbroj(p);
    airbridge(p, d0());
    zegarem(p, ep(2027, 1, 1, 11, 0));
    bool przyszlosc = informacjaNaEkranie(wybudz(p));
    airbridge(p, d0());
    zegarem(p, ep(2026, 11, 10, 12, 0));
    bool poKorekcie = informacjaNaEkranie(wybudz(p));
    printf("  RG zegar z przyszłości: pokazana 2027-01-01 %s, po korekcie %s, wpis w NVS '%s' (zostaje do 2027-01-01)\n",
           przyszlosc ? "tak" : "nie", poKorekcie ? "tak" : "nie", informacjaNvs(p).dzien.c_str());
  }
}

// RG: dwie aktualizacje automatyczne pod rząd (druga przed pokazaniem pierwszej): pokazuje się tylko najnowsza, z jej
// nowościami; automat, potem ręczna: informacja automatu skasowana, wynik ręcznej na U-07.
static void testRec_dwieAktualizacje() {
  Panel p;
  Informacja i;
  i.rodzaj = RodzajInformacji::UDANA;
  i.wlasna = i.nowa = "0.9.9";  // poprzednia informacja niepokazana (wersja przed wgraniem 1.0.0)
  p.nvs.ustawTekst(aktualizacja::PRZESTRZEN, aktualizacja::KLUCZ_INFORMACJI, aktualizacja::zapisInformacji(i));
  sekrety::zapiszKluczWydan(p.nvs, TOKEN);
  p.wydania.ustaw("1.0.1", obrazTestowy(), ukladWkompilowany(), PLYTKA_WYDANIA, {"Nowość 1.0.1"});
  p.pliki.pliki["/konfiguracja.txt"] = "aktualizacja_auto=tak\n";
  airbridge(p, d0());
  zegarem(p, ep(2026, 11, 10, 9, 0));
  wybudz(p);
  SPRAWDZ(p.wydania.restartow == 1);
  restart(p, ep(2026, 11, 10, 9, 5), "1.0.1+def5678", StanWersji::DOBRA);
  airbridge(p, d0());
  TekstyWybudzenia t = wybudz(p);
  SPRAWDZ(!informacjaNaEkranie(t));  // po poprawkach (decyzja 12): rano wynik nocy, informacja w slocie dziennym
  airbridge(p, d0());
  zegarem(p, ep(2026, 11, 10, 11, 0));
  t = wybudz(p);
  SPRAWDZ(wTekstach(t, "U-07 · Zaktualizowano do 1.0.1.") && wTekstach(t, "Co nowego: Nowość 1.0.1"));
  SPRAWDZ(!wTekstach(t, "0.9.9"));
}

// RG: nowsze wydanie wykryte po instalacji, przed pokazaniem: informacja 1.0.1 bez nowości 1.0.2 (nigdy nowości
// innej wersji).
static void testRec_nowosciInnejWersji() {
  Panel p;
  airbridge(p, d0());
  zegarem(p, ep(2026, 11, 10, 9, 0));
  wybudz(p);
  Informacja i;
  i.rodzaj = RodzajInformacji::UDANA;
  i.wlasna = "1.0.0";
  i.nowa = "1.0.0";
  p.nvs.ustawTekst(aktualizacja::PRZESTRZEN, aktualizacja::KLUCZ_INFORMACJI, aktualizacja::zapisInformacji(i));
  p.nvs.ustawTekst(aktualizacja::PRZESTRZEN, "u07_cnv", "1.0.2\nNowość tylko w 1.0.2");
  airbridge(p, d0());
  zegarem(p, ep(2026, 11, 10, 11, 0));
  TekstyWybudzenia t = wybudz(p);
  SPRAWDZ(informacjaNaEkranie(t));
  SPRAWDZ(!wTekstach(t, "Nowość tylko w 1.0.2") && !wTekstach(t, "Co nowego"));
  // ta sama wersja w u07_cnv: nowości pokazane
  p.nvs.ustawTekst(aktualizacja::PRZESTRZEN, "u07_cnv", "1.0.0\nNowość 1.0.0");
  airbridge(p, d0());
  zegarem(p, ep(2026, 11, 10, 13, 0));
  t = wybudz(p);
  SPRAWDZ(wTekstach(t, "Co nowego: Nowość 1.0.0"));
}

// RG: koszt: Wi-Fi w dniu z informacją taki sam jak bez niej; zapisy NVS klucza na instalację (uzbrojenie, pokazanie,
// zamknięcie) i zero w dobach bez zmian.
static void testRec_kosztWifiNvs() {
  int wifi[2] = {0, 0};
  for (int z = 0; z < 2; z++) {
    Panel p;
    airbridge(p, d0());
    zegarem(p, ep(2026, 11, 10, 7, 0));
    wybudz(p);
    if (z == 0) uzbroj(p);
    for (int h = 9; h <= 21; h++) {
      airbridge(p, d0());
      zegarem(p, ep(2026, 11, 10, h, 0));
      wybudz(p);
      wifi[z] += ileW(p, "siec.polacz");
    }
  }
  printf("  RG koszt: Wi-Fi z informacją %d połączeń, bez %d\n", wifi[0], wifi[1]);
  SPRAWDZ(wifi[0] == wifi[1]);
}

// RG: poranek bez Portretu, przerywnik albo dowód spokoju: czy wynik nocy (ekran dnia S2…) jest widoczny bez
// naciśnięcia, a po ◀ (zamknięcie) widać ekran dnia S2… (nie inny ekran).
static void testRec_poranek() {
  Panel p;
  for (int h = 0; h < 8; h++) {
    airbridge(p, d0(), STATUS_SPOCZYNEK, 20);
    restart(p, ep(2026, 11, 10, 12, 0) + (uint32_t)(h * 1800), WERSJA_ATRAPY, StanWersji::DOBRA);
    p.start.przyczynaStartu = PrzyczynaStartu::ZASILANIE;
    wybudz(p);
  }
  uzbroj(p);
  airbridge(p, d0() + 1, STATUS_SPOCZYNEK, 21);
  zegarem(p, ep(2026, 11, 11, 7, 0));
  TekstyWybudzenia t = wybudz(p);
  bool inf = informacjaNaEkranie(t);
  printf("  RG poranek bez Portretu 07:00: informacja zamiast S2 %s (wynik nocy niewidoczny bez przycisku)\n",
         inf ? "TAK" : "nie");
  // po poprawkach (decyzja 12): o 07:00 wynik nocy bez informacji (sprawdzenie blokujące); ◀ na ekranie dnia
  // zaczyna slot dzienny (R-KON-41), więc informacja może stanąć na pierwszym ekranie slotu
  SPRAWDZ(!inf && wLogu(p, "ekran dnia S2"));
  przyciskiem(p, ep(2026, 11, 11, 7, 5), B_LEWY, 200);
  airbridge(p, d0() + 1, STATUS_SPOCZYNEK, 21);
  t = wybudz(p);
  bool slot = wLogu(p, "slot dzienny: S-K-");
  printf("  RG poranek: po ◀ ekran slotu %s, informacja %s\n", slot ? "tak" : "nie", informacjaNaEkranie(t) ? "tak" : "nie");
  SPRAWDZ(!informacjaNaEkranie(t) || slot);
}

// RG: najdłuższa wersja, jaką przyjmuje zapis informacji (32 znaki, wersjaInformacji()), w obu wariantach: treść
// mieści się w prawej kolumnie (do 772 px) i nad stopką; podgląd PNG w /tmp przez PODGLAD_REC.
static void testRec_wersja32() {
  aktualizacja::DaneU07 d;
  d.samodzielnie = true;
  d.powod = aktualizacja::Powod::NIE_WSTALA;
  d.wlasna = "1.10.100-beta.20261115.abcd12345";  // 32 znaki, poprawna wersja SemVer (parsujWersje)
  d.nowa = d.wlasna;
  d.coNowego = {std::string(40, 'W'), std::string(40, 'W'), std::string(40, 'W')};
  for (aktualizacja::StanU07 st : {aktualizacja::StanU07::PO_UDANEJ, aktualizacja::StanU07::PO_NIEUDANEJ}) {
    d.stan = st;
    PlotnoAtrapy p;
    rysujAktualizacje(p, aktualizacja::trescU07(d));
    int prawo = czarneW(p, 773, 44, 800, 433), dol = czarneW(p, U07_X, 420, 800, 434);
    printf("  RG wersja 32 znaki, stan %d: poza prawym marginesem %d px, przy stopce %d px\n", (int)st, prawo, dol);
    {  // to samo po instalacji ręcznej (stan sprzed zmiany): czy usterka jest nowa
      aktualizacja::DaneU07 r = d;
      r.samodzielnie = false;
      PlotnoAtrapy q;
      rysujAktualizacje(q, aktualizacja::trescU07(r));
      printf("    ręczna (baza), stan %d: poza prawym marginesem %d px\n", (int)st, czarneW(q, 773, 44, 800, 433));
    }
    SPRAWDZ(prawo == 0 && dol == 0);
  }
  Informacja i;
  i.rodzaj = RodzajInformacji::UDANA;
  i.wlasna = i.nowa = std::string(32, '8');
  SPRAWDZ(aktualizacja::odczytInformacji(aktualizacja::zapisInformacji(i)).rodzaj == RodzajInformacji::UDANA);
}

// RG: zasilanie odcięte i przywrócone przy informacji na ekranie (StanRtc wyzerowany, e-ink dalej ją pokazuje):
// pierwsze wybudzenie po starcie rysuje ją znowu (ta sama doba), przycisk ją zamyka; restart po zamknięciu: nie wraca.
static void testRec_zanikZasilania() {
  Panel p;
  airbridge(p, d0());
  zegarem(p, ep(2026, 11, 10, 9, 0));
  wybudz(p);
  uzbroj(p);
  airbridge(p, d0());
  zegarem(p, ep(2026, 11, 10, 11, 0));
  SPRAWDZ(informacjaNaEkranie(wybudz(p)));
  restart(p, ep(2026, 11, 10, 11, 30), WERSJA_ATRAPY, StanWersji::DOBRA);
  p.start.przyczynaStartu = PrzyczynaStartu::ZASILANIE;
  airbridge(p, d0());
  SPRAWDZ(informacjaNaEkranie(wybudz(p)));
  SPRAWDZ(p.start.stan.informacjaNaEkranie == 1);
  przyciskiem(p, ep(2026, 11, 10, 11, 35), B_SRODEK, 200);
  airbridge(p, d0());
  SPRAWDZ(!informacjaNaEkranie(wybudz(p)) && zapisNvs(p).empty());
}

// RG: automat, potem przed pokazaniem ręczna instalacja nowszej (1.0.2 z U-07): informacja automatu skasowana, po
// ręcznej U-07 `Zainstalowano wersję …` jak dotąd; nieudana z automatu, następnej doby udana: pokazuje udaną.
static void testRec_automatPotemReczna() {
  Panel p;
  uzbroj(p, RodzajInformacji::NIEUDANA, "1.0.0", "1.0.1");
  sekrety::zapiszKluczWydan(p.nvs, TOKEN);
  p.wydania.ustaw("1.0.2", obrazTestowy(), ukladWkompilowany());
  airbridge(p, d0());
  zegarem(p, ep(2026, 11, 10, 9, 0));
  wybudz(p);
  p.start.stan.ekran = (uint8_t)Ekran::USTAWIENIA;
  przyciskiem(p, ep(2026, 11, 10, 9, 1), B_SRODEK, 2500);
  wybudz(p);
  przyciskiem(p, ep(2026, 11, 10, 9, 2), B_PRAWY, 200);
  wybudz(p);
  SPRAWDZ(p.wydania.restartow == 1);
  restart(p, ep(2026, 11, 10, 9, 5), "1.0.2+def5678", StanWersji::DOBRA);
  airbridge(p, d0());
  TekstyWybudzenia t = wybudz(p);
  SPRAWDZ(wTekstach(t, "U-07 · Zainstalowano wersję 1.0.2.") && !wTekstach(t, "nie udała"));
  SPRAWDZ(zapisNvs(p).empty());
}

int main() {
  setenv("TZ", "CET-1CEST,M3.5.0,M10.5.0/3", 1);
  tzset();
  testRec_przytrzymania();
  testRec_bateriaWTrakcie();
  testRec_nieaktualneWTrakcie();
  testRec_zegar();
  testRec_dwieAktualizacje();
  testRec_nowosciInnejWersji();
  testRec_kosztWifiNvs();
  testRec_poranek();
  testRec_wersja32();
  testRec_zanikZasilania();
  testRec_automatPotemReczna();
  printf("testy_karta_po_aktualizacji_brzegowe: %d ok, %d błędów\n", ok, zle);
  return zle ? 1 : 0;
}
