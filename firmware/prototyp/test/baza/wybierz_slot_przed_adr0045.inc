// SPDX-License-Identifier: GPL-3.0-or-later
// Baza do testów zgodności (ADR-0045, T-TEST-435): wybierzSlot() z src/slot_dzienny.cpp sprzed ADR-0045 (commit e7bb4d4),
// bez zmian. Dołączany wewnątrz `namespace baza { }` w test/testy_kolejnosc_ekranow.cpp; nie kompilować osobno.
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
  bool okno = r.okno.wariant != Wietrzenie::BRAK;
  // w pierwszym bloku slotu S-K-38 przed zwykłymi S-K-33…S-K-37 (R-KON-50); przed nim
  // tylko D0a (wyżej) i `Przewietrz teraz.` (R-KON-44), a z linią uwagi (R-KON-54) obrazka nie ma
  if (w.obrazekJest && !w.liniaUwagi && r.okno.wariant != Wietrzenie::CO2 && wBlokuObrazka(w.minuta, w.slotOd)) {
    r.ekran = EkranSlotu::SK38;
    return r;
  }
  bool jutro = r.jutro.wariant != Jutro::BRAK;
  // R-KON-46: po 16:00 S-K-34 przed S-K-33, chyba że okno trwa albo zaczyna się w ciągu 60 min (wariant CO2 to „teraz”)
  bool oknoBliskie = r.okno.wariant == Wietrzenie::TRWA || r.okno.wariant == Wietrzenie::CO2 ||
                     (r.okno.wariant == Wietrzenie::POZNIEJ && r.okno.od - godz <= 1);
  if (jutro && okno && godz >= JUTRO_PRZED_OKNEM_OD && !oknoBliskie) { r.ekran = EkranSlotu::SK34; return r; }
  if (okno) { r.ekran = EkranSlotu::SK33; return r; }
  if (jutro) { r.ekran = EkranSlotu::SK34; return r; }
  if (w.sprzet) {
    r.wymiana = wymianaSprzetu(*w.sprzet, w.dzis, w.ostatnieSK35);
    if (r.wymiana.element >= 0) { r.ekran = EkranSlotu::SK35; return r; }
  }
  if (r.pylek.rodzaj >= 0) { r.ekran = EkranSlotu::SK36; return r; }
  // D5 poza blokiem obrazka (R-KON-50): S-K-37, a bez współrzędnych miasta zastępstwo S-K-38 (R-KON-54)
  if (w.wspolrzedne) r.slonce = dzienINoc(w.dzis, w.lat, w.lon);
  bool sk37 = r.slonce.jest, sk38 = w.obrazekJest && !w.liniaUwagi;
  r.ekran = sk37 ? EkranSlotu::SK37 : sk38 ? EkranSlotu::SK38 : EkranSlotu::BRAK;
  return r;
}
