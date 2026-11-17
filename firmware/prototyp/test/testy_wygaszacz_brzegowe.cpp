// SPDX-License-Identifier: GPL-3.0-or-later
// Testy brzegowe wygaszacza ekranu (R-KON-73…R-KON-77, 16.11.2026): scenariusze na dyrygencie z atrapami
// (energia doby z wygaszaczem i bez, naciśnięcie tuż przed oceną, drzemka pod wygaszaczem, `ekranOd` z przyszłości),
// rozmiar strony formularza z błędami nowych pól i parser kluczy `dzien.wygaszacz*`.
// Atrapa i pomocnicze funkcje z test/testy_slot_dzienny_dyrygent.cpp (dołączony z przemianowanym main).
// Uruchomienie: tools/prototyp/testy.sh (program testy_wygaszacz_brzegowe).
#include <functional>
int glowna_slotu_dyrygenta();
#define main glowna_slotu_dyrygenta
#include "testy_slot_dzienny_dyrygent.cpp"
#undef main
#include "formularz.h"
#include "sekrety.h"
#include "slot_dzienny.h"

static void obrazkiSym(PanelSym& s) {
  s.p.pliki.pliki["/obrazki/001_syntetyczny.pbm"] = pbm(434, 434);
  s.p.pliki.pliki["/obrazki/obrazki.json"] = "{\"wersja\":1,\"obrazki\":[{\"plik\":\"001_syntetyczny.pbm\"}]}";
}
static int odswiezen(const std::vector<Wyb>& v, int odMin = 0, int doMin = 24 * 60) {
  int n = 0;
  for (auto& w : v)
    if (w.minuta >= odMin && w.minuta < doMin) n += w.m.odswiezen;
  return n;
}

// Doba jak symuluj(), z hakiem przed każdym wybudzeniem (np. zepsuty stan RTC).
static std::vector<Wyb> dobaZHakiem(PanelSym& s, uint32_t od, uint32_t doT,
                                    const std::function<void(PanelSym&, uint32_t)>& hak, const std::string& plikKonf = "") {
  obrazkiSym(s);
  if (!plikKonf.empty()) s.p.pliki.pliki["/konfiguracja.txt"] = plikKonf;
  std::vector<Wyb> v;
  zerujPrzedSnem(s.p.start.l, identyfikatorWersji(s.p.start.napisWersji));
  uint32_t t = od;
  PowodWybudzenia pw = PowodWybudzenia::START;
  const KonfiguracjaPanelu k = konf(true);
  for (int i = 0; i < 800 && t < doT; i++) {
    odpowiedziB9(s, t);
    hak(s, t);
    Wyb w = wybudz(s, t, pw, k);
    v.push_back(w);
    if (!w.spal) break;
    uint32_t snu = s.sen.plan.budzZegarem ? s.sen.plan.sekund : OKRES_SNU_S;
    if (snu == 0) snu = 1;
    t += w.m.msPrzySnie / 1000 + snu;
    pw = PowodWybudzenia::ZEGAR;
  }
  return v;
}

// (c) energia: wygaszacz nie może dodać wybudzeń ani odświeżeń e-ink wobec doby z wyłączonym wygaszaczem
// wymaganie: R-KON-73
static void testRec_energiaDoby() {
  PanelSym a, b;
  DobaWyg on = dobaZNacisnieciami(a, ep(2026, 11, 4, 0, 5), ep(2026, 11, 4, 22, 30), {});
  DobaWyg off = dobaZNacisnieciami(b, ep(2026, 11, 4, 0, 5), ep(2026, 11, 4, 22, 30), {}, "dzien.wygaszacz=nie\n");
  printf("  rec energia bez naciśnięć: wygaszacz %d odświeżeń / %zu wybudzeń, wyłączony %d / %zu\n", odswiezen(on.v),
         on.v.size(), odswiezen(off.v), off.v.size());
  ROWNE_L(on.v.size(), off.v.size());
  SPRAWDZ(odswiezen(on.v) <= odswiezen(off.v));
  ROWNE_L(odswiezen(on.v, 0, 8 * 60), odswiezen(off.v, 0, 8 * 60));  // przed slotem identycznie
  // z naciśnięciami (↻ 11:20 na wygaszaczu, ▶ 15:30, ↻ 18:10)
  std::vector<Nac> n = {{ep(2026, 11, 4, 11, 20), BIT_SRODEK}, {ep(2026, 11, 4, 15, 30), BIT_PRAWY},
                        {ep(2026, 11, 4, 18, 10), BIT_SRODEK}};
  PanelSym c, d;
  DobaWyg onN = dobaZNacisnieciami(c, ep(2026, 11, 4, 0, 5), ep(2026, 11, 4, 22, 30), n);
  DobaWyg offN = dobaZNacisnieciami(d, ep(2026, 11, 4, 0, 5), ep(2026, 11, 4, 22, 30), n, "dzien.wygaszacz=nie\n");
  printf("  rec energia z 3 naciśnięciami: wygaszacz %d / %zu, wyłączony %d / %zu\n", odswiezen(onN.v), onN.v.size(),
         odswiezen(offN.v), offN.v.size());
  ROWNE_L(onN.v.size(), offN.v.size());
  SPRAWDZ(odswiezen(onN.v) <= odswiezen(offN.v));
}

// (b) naciśnięcie ↻ 30 s przed oceną o 11:00: licznik od 10:59, wygaszacz dopiero o 14:00 (181 min), nie o 13:00
// wymaganie: R-KON-76
static void testRec_przyciskTuzPrzedOcena() {
  PanelSym s;
  DobaWyg d = dobaZNacisnieciami(s, ep(2026, 11, 4, 0, 5), ep(2026, 11, 4, 15, 30), {{ep(2026, 11, 4, 10, 59, 30), BIT_SRODEK}});
  SPRAWDZ(ekranO(d.v, "S-K-37", 11) && ekranO(d.v, "S-K-37", 13));
  SPRAWDZ(!logMa(d.v, "slot dzienny: S-K-38", 0, 14 * 60));
  SPRAWDZ(ekranO(d.v, "S-K-38", 14));
}

// (a) drzemka pod wygaszaczem: o 16:00 drzemka trwa (ekran bez zmian, bez odświeżenia), o 17:00 S-K-39 z wyciekiem
// wraca na wierzch (R-KON-58, R-KON-75)
// wymaganie: R-KON-75
static void testRec_drzemkaPodWygaszaczem() {
  PanelSym s;
  obrazkiSym(s);
  std::vector<Wyb> v = symuluj(s, ep(2026, 11, 4, 0, 5), ep(2026, 11, 4, 17, 30), odpDrzemki, false, konf(true));
  SPRAWDZ(ekranO(v, "S-K-38", 11) && ekranO(v, "S-K-38", 15));
  SPRAWDZ(logMa(v, "drzemka trwa (rop 1)", 16 * 60, 16 * 60 + 1));
  ROWNE_L(odswiezen(v, 16 * 60, 16 * 60 + 1), 0);
  SPRAWDZ(ekranO(v, "S-K-39", 17));
  ROWNE_L(odswiezen(v, 17 * 60, 17 * 60 + 1), 1);
}

// (b) zepsuta pamięć RTC: `ekranOd` daleko w przyszłości (np. po korekcie NTP wstecz o rok albo śmieciach w RTC) od
// 09:00. Oczekiwanie: wygaszacz wraca najpóźniej po progu od chwili, gdy panel to zauważy, a nie dopiero
// po naciśnięciu przycisku (decyzja 1: „wchodzi po 3 godzinach bez naciśnięcia”).
// wymaganie: R-KON-74
static void testRec_ekranOdZPrzyszlosci() {
  PanelSym s;
  bool zepsute = false;
  std::vector<Wyb> v = dobaZHakiem(s, ep(2026, 11, 4, 0, 5), ep(2026, 11, 4, 20, 30), [&](PanelSym& p, uint32_t t) {
    if (!zepsute && t >= ep(2026, 11, 4, 9, 0)) {
      p.p.start.stan.ekranOd = t + 400u * 24u * 3600u;
      zepsute = true;
    }
  });
  printf("  rec ekranOd z przyszłości od 09:00: S-K-38 o 12:00 %d, o 15:00 %d, o 20:00 %d\n", ekranO(v, "S-K-38", 12),
         ekranO(v, "S-K-38", 15), ekranO(v, "S-K-38", 20));
  SPRAWDZ(logMa(v, "slot dzienny: S-K-38", 12 * 60, 21 * 60));
}

// (e) rozmiar strony: scenariusz brzegowy (każde pole z błędem 91 B) rozszerzony o pola, które mają miejsce na
// błąd, a nie były w liście: `wygaszacz`, `wygaszacz_min`, `ekrany_start` (wszystkie trzy wypisuje sekcjaEkranow()).
// Strona ma się mieścić w REZERWACJA_STRONY bez realokacji.
// wymaganie: R-KON-77
static void testRec_stronaZBledamiWygaszacza() {
  sekrety::Dane g;
  g.ssid = std::string(32, '"');
  g.hasloWifi = "x";
  g.abAdres = std::string(64, '"');
  g.abKonto = std::string(32, '"');
  g.abHaslo = "x";
  formularz::Pola gr = formularz::zPliku(
      "miasto=Gorzów Wielkopolski\nwymiana.maska=2026-09-02\nwymiana.filtr=2026-09-02\nokres.maska=60\n"
      "okres.filtr_tyg=104\nraport.czynniki=KLABCDEFGHIJKLMNOPQRSTUVWXYZ\nkokpit.kolejnosc=sypialnia,7_nocy\n"
      "kokpit.ukryte=sypialnia\ndzien.ukryte=wietrzenie,jutro_rano,wymiana,pylek,dzien_i_noc,obrazek\n"
      "dzien.wygaszacz_min=720\n",
      g, true);
  gr.hasloWifi = gr.hasloAb = gr.hasloOkna = true;
  gr.nowyModelDzis = true;
  gr.termin[0] = gr.termin[1] = "po terminie od 02.03.2027";
  gr.wersja = "10.10.100-rc.100+g1234567";
  std::string kom = "AirBridge pod " + std::string(64, '"') +
                    " odrzucił logowanie. Sprawdź konto i hasło. Poprzednie ustawienia zostają. Nie udało się zapisać "
                    "w pamięci panelu.";
  std::map<std::string, std::string> wszystkie;
  for (const char* pole : {"ssid", "wifi_haslo", "wifi_bez", "ab_adres", "ab_konto", "ab_haslo", "profil", "co2", "miasto",
                           "humor", "buzzer", "cisza_od", "cisza_do", "podpowiedz", "cz_start", "wymiana.maska",
                           "okres.maska", "nowy_model", "nowy_model_cofnij", "wymiana.filtr", "okres.filtr_tyg",
                           "uwaga_o_masce", "aktualizacja_auto", "klucz_wydan", "klucz_usun", "haslo_okna",
                           "kk_ostatnia_noc", "kk_7_nocy", "kk_sypialnia", "dk_wietrzenie", "dk_jutro_rano", "dk_wymiana",
                           "dk_pylek", "ekrany_start", "wygaszacz", "wygaszacz_min"})
    wszystkie[pole] = "Nowa sieć: wpisz jej hasło albo zaznacz „bez hasła”. Panel nie zna dzisiejszej daty.";
  std::string sr = formularz::strona(gr, "/u/ABCDEFGHIJKLMNOPQRSTUVWXYZ234567/", kom, wszystkie);
  printf("  rec strona: wszystkie pola z błędem łącznie z wygaszaczem %zu B, rezerwacja %zu B, zapas %ld B\n", sr.size(),
         formularz::REZERWACJA_STRONY, (long)formularz::REZERWACJA_STRONY - (long)sr.size());
  SPRAWDZ(sr.size() <= formularz::REZERWACJA_STRONY);
  SPRAWDZ(sr.find("value=\"720\"") != std::string::npos);
}

// (d) parser kluczy: wielkość liter, duplikat, spacje, zera wiodące
// wymaganie: R-KON-74
static void testRec_parser() {
  UstawieniaDnia a = parsujUstawieniaDnia("dzien.wygaszacz=NIE\n");
  SPRAWDZ(a.wygaszaczWlaczony() && !a.bledy.empty());  // zła wartość: włączony, wpis w błędach
  UstawieniaDnia b = parsujUstawieniaDnia("dzien.wygaszacz_min=0060\n");
  ROWNE_L(b.wygaszaczMinSkuteczne(), 60);
  UstawieniaDnia c = parsujUstawieniaDnia("dzien.wygaszacz_min=60\ndzien.wygaszacz_min=90\n");
  printf("  rec parser: duplikat 60/90 → %d, błędów %zu\n", c.wygaszaczMinSkuteczne(), c.bledy.size());
  UstawieniaDnia d = parsujUstawieniaDnia("dzien.wygaszacz_min=60\ndzien.wygaszacz_min=10\n");
  printf("  rec parser: 60 potem zła 10 → %d\n", d.wygaszaczMinSkuteczne());
  for (const char* v : {"29", "721", "", "abc", "-30", "1e2", "99999999999999999999"}) {
    UstawieniaDnia e = parsujUstawieniaDnia(std::string("dzien.wygaszacz_min=") + v + "\n");
    ROWNE_L(e.wygaszaczMinSkuteczne(), 180);
  }
  ROWNE_L(parsujUstawieniaDnia("dzien.wygaszacz_min=30\n").wygaszaczMinSkuteczne(), 30);
  ROWNE_L(parsujUstawieniaDnia("dzien.wygaszacz_min=720\n").wygaszaczMinSkuteczne(), 720);
}

// (b) granice licznika na poziomie funkcji: start slotu 08:00:59, ocena 11:00:00 = 179 min 1 s, a wygaszacz już wchodzi
// (minuty ucięte); zapis jako wiedza o zachowaniu, nie usterka
static void testRec_granice() {
  const uint32_t s = 1'800'000'000u;  // pełna minuta
  printf("  rec granice: start +59 s, ocena +180 min → %d (prawdziwa bezczynność 179 min 1 s)\n",
         poraWygaszacza(s + 180 * 60, s + 59, 0, 180));
  // naciśnięcie dokładnie w chwili oceny: bez wygaszacza
  SPRAWDZ(!poraWygaszacza(s + 300 * 60, s, s + 300 * 60, 180));
}

int main() {
  setenv("TZ", STREFA_PANELU, 1);
  tzset();
  testRec_granice();
  testRec_parser();
  testRec_stronaZBledamiWygaszacza();
  testRec_energiaDoby();
  testRec_przyciskTuzPrzedOcena();
  testRec_drzemkaPodWygaszaczem();
  testRec_ekranOdZPrzyszlosci();
  printf("testy_wygaszacz_brzegowe: %d ok, %d błędów\n", ok, zle);
  return zle ? 1 : 0;
}
