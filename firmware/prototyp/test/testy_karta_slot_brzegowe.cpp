// SPDX-License-Identifier: GPL-3.0-or-later
// Testy brzegowe: informacja po aktualizacji
// samodzielnej wobec ekranów slotu dziennego ze sprawą (`Przewietrz teraz.`, S-K-39) i ostrzeżeń powstałych w trakcie
// doby, w której informacja stoi na ekranie. Harness z test/testy_slot_dzienny_dyrygent.cpp (main przemianowany).
// Od rundy poprawek (15.11.2026) test blokujący w tools/prototyp/testy.sh.
#define main main_testy_slot_dzienny_dyrygent
#include "testy_slot_dzienny_dyrygent.cpp"
#undef main

#include "aktualizacja.h"

static void uzbrojRec(PanelSym& s) {
  aktualizacja::Informacja i;
  i.rodzaj = aktualizacja::RodzajInformacji::UDANA;
  i.wlasna = i.nowa = "1.0.0";
  s.p.nvs.ustawTekst(aktualizacja::PRZESTRZEN, aktualizacja::KLUCZ_INFORMACJI, aktualizacja::zapisInformacji(i));
}
static bool jestW(const Wyb& w, const std::string& t) {
  for (auto& z : w.zdarzenia)
    if (z.find(t) != std::string::npos) return true;
  return false;
}
static const char* const INF = "ekran dnia: informacja po aktualizacji samodzielnej";

static int co2Rec = 1250;
static uint32_t co2RecOd = 0;  // od tej chwili CO2 1250, wcześniej 600
static void odpCo2(PanelSym& s, uint32_t t) {
  odpowiedziB9(s, t);
  s.siec.odpowiedzi.erase("/otoczenie");
  int c = t >= co2RecOd ? co2Rec : 600;
  s.siec.dodaj("/otoczenie", 200, "{\"wersja\":1,\"sypialnia\":{\"co2\":" + std::to_string(c) + "},\"ostrzezenia\":[]}");
}
// Przebieg doby z informacją uzbrojoną przed startem; zwraca wybudzenia.
static std::vector<Wyb> doba(PanelSym& s, Odpowiedzi odp, uint32_t doT, const KonfiguracjaPanelu& k, bool zInformacja) {
  if (zInformacja) uzbrojRec(s);
  return symuluj(s, ep(2026, 11, 4, 0, 5), doT, odp, false, k);
}

// `Przewietrz teraz.` (S-K-33 w wariancie CO2, R-KON-72: ekran ostrzegawczy „zawsze włączony”, przed listą)
// zasłonięty informacją. (a) CO2 wysokie od rana; (b) CO2 rośnie o 12:00, gdy informacja stoi od 08:00.
// Wariant CO2 rozpoznany w osobnym przebiegu bez informacji (log `slot dzienny: S-K-33` przy CO2 1250 i czystym
// powietrzu; w kopii z dopiskiem tytułu do logu: `Przewietrz teraz.`).
static void testRG01_przewietrz() {
  KonfiguracjaPanelu k = konf(true);
  k.otoUrl = "http://192.168.1.60:8080/otoczenie";
  for (int wariant = 0; wariant < 2; wariant++) {
    co2RecOd = wariant == 0 ? 0 : ep(2026, 11, 4, 12, 0);
    PanelSym s;
    std::vector<Wyb> v = doba(s, odpCo2, ep(2026, 11, 4, 14, 30), k, true);
    int zaslonietych = 0, slotowCo2 = 0;
    for (auto& w : v) {
      if (w.ep < co2RecOd || w.minuta < 8 * 60) continue;
      if (!jestW(w, "slot dzienny: S-K-33")) continue;
      slotowCo2++;
      if (jestW(w, INF)) zaslonietych++;
    }
    printf(" %s: wybudzeń z S-K-33 przy CO2 1250: %d, z informacją zamiast niego: %d\n",
           wariant == 0 ? "CO2 od rana" : "CO2 od 12:00 (informacja stoi od rana)", slotowCo2, zaslonietych);
    SPRAWDZ(slotowCo2 > 0);
    SPRAWDZ(zaslonietych == 0);
  }
}

// S-K-39 (drzemka z wyciekiem) powstałe w trakcie doby z informacją: informacja ustępuje (sprawdzone, bez usterki).
static void testRec_sk39WTrakcie() {
  PanelSym s;
  std::vector<Wyb> v = doba(s, odpDrzemki, ep(2026, 11, 4, 17, 30), konf(true), true);
  bool infRano = logMa(v, INF, 8 * 60, 16 * 60);
  bool sk39 = logMa(v, "slot dzienny: S-K-39", 17 * 60, 17 * 60 + 1);
  bool inf17 = logMa(v, INF, 17 * 60, 17 * 60 + 1);
  printf("  S-K-39 w trakcie doby: informacja przed 16:00 %d, S-K-39 o 17:00 %d, informacja o 17:00 %d\n", infRano, sk39,
         inf17);
  SPRAWDZ(sk39 && !inf17);
}

// Przycisk na informacji rano zaczyna slot dzienny (R-KON-41, startSlotuPrzyciskiem przed obsługą informacji):
// po zamknięciu informacji ◀ użytkownik widzi ekran slotu zamiast ekranu dnia z wynikiem nocy.
static void testRec_poranekPrzyciskSlot() {
  KonfiguracjaPanelu k = konf(true);
  for (int z = 0; z < 2; z++) {
    PanelSym s;
    if (z == 0) uzbrojRec(s);
    std::vector<Wyb> v = symuluj(s, ep(2026, 11, 4, 0, 5), ep(2026, 11, 4, 7, 10), odpowiedziB9, false, k);
    bool inf7 = logMa(v, INF, 7 * 60, 7 * 60 + 10);
    // ◀ o 07:15
    odpowiedziB9(s, ep(2026, 11, 4, 7, 15));
    s.p.start.budzace = 1u << (int)Przycisk::LEWY;
    s.p.przyciski.skrypt = {{0, s.p.start.budzace}, {200, 0}};
    Wyb w = wybudz(s, ep(2026, 11, 4, 7, 15), PowodWybudzenia::PRZYCISK, k);
    bool slot = jestW(w, "slot dzienny: S-K-");
    bool startSlotu = jestW(w, "slot dzienny: start naciśnięciem");
    bool dzien = jestW(w, "ekran dnia S");
    printf("  poranek %s: informacja 07:00 %d; po ◀ 07:15: start slotu %d, ekran slotu %d, ekran dnia S… %d\n",
           z == 0 ? "z informacją" : "bez informacji (baza)", inf7, startSlotu, slot, dzien);
    SPRAWDZ(!inf7);  // po poprawkach (decyzja 12): rano wynik nocy, nigdy informacja
  }
}

// Koszt w slocie dziennym (wybudzenia co godzinę z rysowaniem slotu): zapisy NVS klucza informacji w dobie (tylko
// pierwsze pokazanie), odświeżenia e-ink i połączenia Wi-Fi wobec tej samej doby bez informacji.
static void testRec_slotKoszt() {
  KonfiguracjaPanelu k = konf(true);
  k.otoUrl = "http://192.168.1.60:8080/otoczenie";
  co2RecOd = 0xFFFFFFFFu;  // CO2 600 przez całą dobę: S-K-33 bez wariantu CO2
  int odsw[2] = {0, 0}, wifi[2] = {0, 0}, zap = 0, zInf = 0;
  for (int z = 0; z < 2; z++) {
    PanelSym s;
    std::vector<Wyb> v = doba(s, odpCo2, ep(2026, 11, 4, 22, 0), k, z == 0);
    for (auto& w : v) {
      if (w.minuta < 7 * 60) continue;
      odsw[z] += w.m.odswiezen;
      wifi[z] += w.m.polaczen;
      if (z == 0) {
        for (auto& e : w.zdarzenia) zap += e == "nvs.zapis aktual/u07_inf";
        zInf += jestW(w, INF);
      }
    }
  }
  printf("  slot 07:00–22:00: wybudzeń z informacją %d, zapisy NVS u07_inf %d, odświeżenia %d (bez informacji %d), "
         "Wi-Fi %d (bez %d)\n", zInf, zap, odsw[0], odsw[1], wifi[0], wifi[1]);
  SPRAWDZ(zap == 1);
  SPRAWDZ(odsw[0] <= odsw[1] + 1);
  SPRAWDZ(wifi[0] == wifi[1]);
  co2RecOd = 0;
}

int main() {
  setenv("TZ", STREFA_PANELU, 1);
  tzset();
  testRG01_przewietrz();
  testRec_sk39WTrakcie();
  testRec_poranekPrzyciskSlot();
  testRec_slotKoszt();
  printf("testy_karta_slot_brzegowe: %d ok, %d błędów\n", ok, zle);
  return zle ? 1 : 0;
}
