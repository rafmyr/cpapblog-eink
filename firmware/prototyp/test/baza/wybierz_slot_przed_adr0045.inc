// SPDX-License-Identifier: GPL-3.0-or-later
// Baza do testów zgodności (ADR-0045, T-TEST-435): wybierzSlot() z src/slot_dzienny.cpp sprzed ADR-0045 (commit e7bb4d4),
// bez zmian w D0a, D1…D4 i kolejności. Zmiana 2026-11-16 (wygaszacz, R-KON-73, R-KON-75; R-KON-50 wycofane, decyzja
// właściciela projektu): bez bloku obrazka od startu slotu. Wygaszacz (decyzja 22:08, W3, zasłanianie dosłowne)
// jest tu NAKŁADKĄ na wynik starej selekcji, napisaną z listy W3, a nie z pętli D1…D4 nowego kodu: gdy po progu
// bezczynności (obrazek jest, bez linii uwagi) stara selekcja daje ekran z listy „zasłania” (S-K-33 `Wietrz od …`
// i `Dziś bez wietrzenia.`, S-K-34, S-K-36, D5 i ekran z rana), wynikiem jest S-K-38; w innym razie wynik starej
// selekcji bez zmian. Dołączany wewnątrz `namespace baza { }` w test/testy_kolejnosc_ekranow.cpp; nie kompilować osobno.
WyborSlotu wybierzSlot(const WejscieSlotu& w) {
  WyborSlotu r;
  int godz = w.minuta >= 0 ? w.minuta / 60 : -1;
  if (wyciekDrzemki(w.drzemka)) {  // D0a (R-KON-58)
    r.ekran = EkranSlotu::SK39;
    return r;
  }
  if (w.prognoza && w.prognoza->jest()) {
    WejscieWietrzenia ww;
    ww.prognoza = w.prognoza;
    ww.dzis = w.dzis;
    ww.godzina = godz;
    ww.przyLozku = w.przyLozku;
    ww.co2 = w.co2;
    ww.pylki = w.pylki;
    r.okno = regulaWietrzenia(ww);
    r.jutro = jutroRano(*w.prognoza, w.dzis);
    r.pylek = pylekDnia(*w.prognoza, w.dzis, godz, w.pylki);
  }
  const bool sk38 = w.obrazekJest && !w.liniaUwagi;
  const bool wyg = sk38 && w.bezczynnosc;
  // nakładka wygaszacza na wynik starej selekcji (lista W3, „zasłania”)
  auto wynik = [&](EkranSlotu e) {
    const bool zasl = e == EkranSlotu::SK34 || e == EkranSlotu::SK36 ||
                      (e == EkranSlotu::SK33 &&
                       (r.okno.wariant == Wietrzenie::POZNIEJ || r.okno.wariant == Wietrzenie::SMOG));
    if (wyg && zasl) {
      if (w.wspolrzedne) r.slonce = dzienINoc(w.dzis, w.lat, w.lon);
      r.wygaszacz = true;
      e = EkranSlotu::SK38;
    }
    r.ekran = e;
    return r;
  };
  bool okno = r.okno.wariant != Wietrzenie::BRAK;
  // (dawny blok obrazka usunięty 16.11.2026, zob. R-KON-73)
  bool jutro = r.jutro.wariant != Jutro::BRAK;
  // R-KON-46: po 16:00 S-K-34 przed S-K-33, chyba że okno trwa albo zaczyna się w ciągu 60 min (wariant CO2 to „teraz”)
  bool oknoBliskie = r.okno.wariant == Wietrzenie::TRWA || r.okno.wariant == Wietrzenie::CO2 ||
                     (r.okno.wariant == Wietrzenie::POZNIEJ && r.okno.od - godz <= 1);
  if (jutro && okno && godz >= JUTRO_PRZED_OKNEM_OD && !oknoBliskie) return wynik(EkranSlotu::SK34);
  if (okno) return wynik(EkranSlotu::SK33);
  if (jutro) return wynik(EkranSlotu::SK34);
  if (w.sprzet) {
    r.wymiana = wymianaSprzetu(*w.sprzet, w.dzis, w.ostatnieSK35);
    if (r.wymiana.element >= 0) return wynik(EkranSlotu::SK35);
  }
  if (r.pylek.rodzaj >= 0) return wynik(EkranSlotu::SK36);
  // D5: S-K-37, a bez współrzędnych miasta zastępstwo S-K-38 (R-KON-54); po progu bezczynności wygaszacz S-K-38
  // (zasłania D5 i ekran z rana)
  if (w.wspolrzedne) r.slonce = dzienINoc(w.dzis, w.lat, w.lon);
  bool sk37 = r.slonce.jest;
  r.wygaszacz = wyg;
  r.ekran = wyg ? EkranSlotu::SK38 : sk37 ? EkranSlotu::SK37 : sk38 ? EkranSlotu::SK38 : EkranSlotu::BRAK;
  return r;
}
