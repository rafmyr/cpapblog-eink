// SPDX-License-Identifier: GPL-3.0-or-later
// Testy: raport A4 dla lekarza (5.13), kod QR (R-LEK-06), serwer
// raportu (R-LEK-02…R-LEK-04, R-LEK-11, R-LEK-20, R-LEK-21, R-LEK-32, R-LEK-33), ekran-ściąga (R-LEK-18), podpięcie
// w dyrygencie (▶ na ekranie L i w K2). Dane syntetyczne. Uruchomienie: tools/prototyp/testy.sh (program
// testy_raport_a4 z katalogiem na przykładowe PDF, domyślnie /tmp/cpap-eink-raport).
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <set>
#include <string>
#include <vector>

#include "adaptery_atrapy.h"
#include "airbridge.h"
#include "dyrygent.h"
#include "nawigacja.h"
#include "qr.h"
#include "raport.h"
#include "raport_ekran.h"
#include "serwer_raportu.h"
#include "sprawy.h"
#include "wspolne.h"
#include "wybudzenie.h"
#include "wzorce.h"
#include "zestaw_kolejki.h"

static int ok = 0, zle = 0;
#define SPRAWDZ(w)                                                         \
  do {                                                                     \
    if (w) ok++;                                                           \
    else { zle++; printf("BŁĄD %s:%d: %s\n", __FILE__, __LINE__, #w); }   \
  } while (0)
#define ROWNE(a, b)                                                                                     \
  do {                                                                                                  \
    std::string _a = (a), _b = (b);                                                                     \
    if (_a == _b) ok++;                                                                                 \
    else { zle++; printf("BŁĄD %s:%d: %s = \"%s\", oczekiwane \"%s\"\n", __FILE__, __LINE__, #a, _a.c_str(), _b.c_str()); } \
  } while (0)

using reguly::Pilnosc;
using reguly::StanReguly;

static const std::string DZIS = "2026-11-04";
static std::string KATALOG = "/tmp/cpap-eink-raport";

static bool zawiera(const std::vector<std::string>& w, const std::string& f) {
  for (auto& x : w)
    if (x.find(f) != std::string::npos) return true;
  return false;
}
static int indeks(const std::vector<std::string>& w, const std::string& f) {
  for (size_t i = 0; i < w.size(); i++)
    if (w[i].find(f) != std::string::npos) return (int)i;
  return -1;
}
static void zapisz(const std::string& nazwa, const std::string& t) {
  std::ofstream(KATALOG + "/" + nazwa, std::ios::binary) << t;
}

// 90 nocy syntetycznych w trybie `tryb`; od nocy 60 CA/h rośnie (wzorzec L-01), co 17. noc krótka, noc 23 z wyciekiem
static std::vector<Noc> historia(const std::string& tryb, int ile = 90) {
  std::vector<Noc> h;
  std::string d0 = dodajDni(DZIS, -(ile - 1));
  for (int i = 0; i < ile; i++) {
    Noc n;
    n.data = dodajDni(d0, i);
    bool krotka = i % 17 == 5;
    n.wazna = !krotka;
    n.m01 = krotka ? 90 : 400 + (i * 7) % 60;
    n.ahi = 3.0f + (i > 60 ? (float)(i - 60) * 0.12f : 0.0f) + (float)(i % 5) * 0.2f;
    n.ca = 1.5f + (i > 60 ? (float)(i - 60) * 0.18f : 0.0f);
    n.oa = 0.6f;
    n.h = 0.8f;
    n.ua = 0.1f;
    n.cmed = 9.8f;
    n.cp95 = 12.4f;
    n.emed = 7.2f;
    n.ep95 = 8.4f;
    n.psmed = 5;
    n.psp95 = 9;
    n.wp95 = 8 + (float)(i % 7);  // 8–14 L/min: w wariancie C P95 ≥ 15 czyni noc niepewną
    n.m17 = 1;
    n.m08 = i == 23 ? 50.0f : 0.0f;
    n.tryb = tryb;
    n.pna = tryb.compare(0, 3, "ASV") == 0 ? "AirCurve_10_CS_PaceWave" : "AirSense_10_AutoSet";
    n.eai = 6;
    n.eax = i < 45 ? 10.0f : 11.0f;
    n.ans = 3;
    n.axs = 15;
    n.eep = 7;
    n.mns = 3;
    n.mxs = 12;
    n.mpi = 6;
    n.mpa = i < 45 ? 14.0f : 15.0f;
    n.ipc = 9;
    n.epr = 2;
    h.push_back(n);
  }
  return h;
}

static sprawy::SprawaL sprawaL01() {
  sprawy::SprawaL s;
  s.klucz = s.id = "L-01";
  s.pilnosc = Pilnosc::A;
  s.ton = sprawy::Ton::LEKARZ;
  s.tytul = "Bezdechy centralne utrwalone";
  s.powod = "Centralne średnio 6,7 / h z 7 nocy, zwykle 1,8.";
  s.aktywnaOd = "2026-10-20";
  return s;
}

// ---------- kod QR ----------
static void testQr() {
  qr::Kod k = qr::koduj("http://192.168.1.47/r/0123456789abcdefghjkmnpqrs/");
  SPRAWDZ(k.rozmiar == 17 + 4 * k.wersja && k.wersja == 4);  // 52 bajty w wersji 4-M (62 bajty)
  // wzorce wykrywania w trzech rogach: ramka 7×7, środek 3×3, separator jasny
  for (int ox : {0, k.rozmiar - 7})
    for (int oy : {0, k.rozmiar - 7}) {
      if (ox && oy) continue;
      SPRAWDZ(k.ciemny(ox, oy) && k.ciemny(ox + 6, oy + 6) && !k.ciemny(ox + 1, oy + 1) && k.ciemny(ox + 3, oy + 3));
    }
  for (int i = 8; i < k.rozmiar - 8; i++) SPRAWDZ(k.ciemny(i, 6) == (i % 2 == 0));  // wzorzec synchronizacji
  SPRAWDZ(k.ciemny(8, k.rozmiar - 8));  // moduł zawsze ciemny
  qr::Kod k2 = qr::koduj("http://192.168.1.47/r/0123456789abcdefghjkmnpqrs/");
  SPRAWDZ(k.moduly == k2.moduly);  // ten sam adres, ten sam kod
  SPRAWDZ(qr::koduj("A").wersja == 1);
  SPRAWDZ(qr::koduj(std::string(213, 'x')).wersja == 10);
  SPRAWDZ(qr::koduj(std::string(214, 'x')).rozmiar == 0);  // za długi: bez kodu
  for (int m = 0; m < 8; m++) SPRAWDZ(qr::koduj("http://x/", m).maska == m);
}

// ---------- token (T-TEST-018 w części rdzenia: adapter podaje bajty, rdzeń buduje token) ----------
static void testToken() {
  std::set<std::string> t;
  uint32_t x = 2463534242u;  // xorshift jako atrapa generatora sprzętowego
  for (int i = 0; i < 100; i++) {
    uint8_t b[serwer::BAJTY_TOKENU];
    for (auto& c : b) {
      x ^= x << 13;
      x ^= x >> 17;
      x ^= x << 5;
      c = (uint8_t)x;
    }
    std::string s = serwer::token(b);
    SPRAWDZ(s.size() == 26);  // 128 bitów w base32
    for (char c : s) SPRAWDZ((c >= '0' && c <= '9') || (c >= 'a' && c <= 'z' && c != 'i' && c != 'l' && c != 'o' && c != 'u'));
    t.insert(s);
  }
  SPRAWDZ(t.size() == 100);
  uint8_t z[serwer::BAJTY_TOKENU] = {};
  ROWNE(serwer::token(z), std::string(26, '0'));
}

// ---------- serwer: trasy, 15 min, 404, 5 złych żądań, lista 90 dni, starsze ----------
static void testSerwer() {
  std::vector<serwer::Pozycja> lista = serwer::pozycjeZPlikow(
      {"/raporty/2026-10-25-L-03.pdf", "/raporty/2026-09-05-ogolny.pdf", "/raporty/2026-04-18-L-11.pdf",
       "/raporty/notatka.txt", "/raporty/2026-11-04-L-01.pdf"});
  SPRAWDZ(lista.size() == 4 && lista[0].data == "2026-11-04");  // od najnowszej, inne nazwy pominięte
  serwer::Sesja s;
  s.otworz("tok", 1000, "192.168.1.47", serwer::Tryb::PDF, "/raporty/2026-11-04-L-01.pdf", lista, DZIS, "Temat",
           "07.08–04.11", false);
  ROWNE(s.adresQr(), "http://192.168.1.47/r/tok/");
  ROWNE(s.adresReczny(), "192.168.1.47/r/tok");
  serwer::Odpowiedz o = s.obsluz("/r/tok/", 2000);
  SPRAWDZ(o.kod == 200 && o.typ.find("text/html") == 0);
  for (const char* f : {"Pobierz PDF", "Udostępnij", "Wysyłając PDF mailem, podaj w wiadomości imię, nazwisko i datę urodzenia.",
                        serwer::EDF_NIEDOSTEPNA, "2026-10-25-L-03.pdf", "2026-09-05-ogolny.pdf"})
    SPRAWDZ(o.cialo.find(f) != std::string::npos);
  // bez paczki EDF ani linku do niej, ani zdania o awarii AirBridge
  SPRAWDZ(o.cialo.find("AirBridge nie odpowiada") == std::string::npos && o.cialo.find("/edf") == std::string::npos);
  SPRAWDZ(o.cialo.find("Paczka EDF dla lekarza") == std::string::npos && !s.edf());
  SPRAWDZ(o.cialo.find("2026-04-18-L-11.pdf") == std::string::npos);  // R-LEK-32: starszy niż 90 dni
  SPRAWDZ(o.cialo.find("192.168.1.50") == std::string::npos && o.cialo.find("/api/") == std::string::npos);  // bez AirBridge
  o = s.obsluz("/r/tok/raport.pdf", 3000);
  SPRAWDZ(o.kod == 200 && o.plik == "/raporty/2026-11-04-L-01.pdf" && o.pdfPobrany && o.typ == "application/pdf");
  SPRAWDZ(s.obsluz("/r/tok/p/2026-04-18-L-11.pdf", 3000).kod == 404);  // R-LEK-32 z ważnym tokenem
  SPRAWDZ(s.zleZadania() == 0);
  s.pokazStarsze();  // R-LEK-33
  o = s.obsluz("/r/tok/p/2026-04-18-L-11.pdf", 3000);
  SPRAWDZ(o.kod == 200 && o.plik == "/raporty/2026-04-18-L-11.pdf");
  SPRAWDZ(s.obsluz("/r/tok/edf", 3000).edf);
  // R-LEK-03, R-LEK-03b: bez tokenu i ze starym tokenem 404; piąte złe żądanie zamyka serwer
  for (int i = 0; i < 4; i++) SPRAWDZ(s.obsluz(i % 2 ? "/" : "/r/stary/", 4000).kod == 404);
  SPRAWDZ(s.aktywna(4000));
  SPRAWDZ(s.obsluz("/r/tokX/", 4000).kod == 404);
  SPRAWDZ(!s.aktywna(4000) && s.zamknietaZlymi());
  SPRAWDZ(s.obsluz("/r/tok/", 4000).kod == 404);
  // R-LEK-04: 15 min od włączenia
  serwer::Sesja t;
  t.otworz("nowy", 0, "192.168.1.47", serwer::Tryb::PDF, "/raporty/a.pdf", {}, DZIS, "T", "", true);
  SPRAWDZ(t.aktywna(serwer::CZAS_MS - 1) && !t.aktywna(serwer::CZAS_MS));
  {  // z paczką EDF: link jest, zdania o niedostępności nie ma
    std::string c = t.obsluz("/r/nowy/", serwer::CZAS_MS - 1).cialo;
    SPRAWDZ(t.edf() && c.find("AirBridge nie odpowiada") == std::string::npos);
    SPRAWDZ(c.find("Paczka EDF dla lekarza (OSCAR/SleepHQ)") != std::string::npos && c.find("/r/nowy/edf") != std::string::npos);
    SPRAWDZ(c.find(serwer::EDF_NIEDOSTEPNA) == std::string::npos);
  }
  SPRAWDZ(t.obsluz("/r/nowy/", serwer::CZAS_MS).kod == 404);
  SPRAWDZ(t.starsze() == false);  // nowa sesja: znów tylko 90 dni (R-LEK-33)
  // tylko adres prywatny; token porównywany w stałym czasie
  SPRAWDZ(serwer::adresPrywatny("192.168.1.47") && serwer::adresPrywatny("10.0.0.2") && serwer::adresPrywatny("172.20.1.1"));
  SPRAWDZ(!serwer::adresPrywatny("8.8.8.8") && !serwer::adresPrywatny("172.32.0.1") && !serwer::adresPrywatny("") &&
          !serwer::adresPrywatny("192.168.1") && !serwer::adresPrywatny("192.168.1.256"));
  SPRAWDZ(serwer::rowneStalyCzas("abc", "abc") && !serwer::rowneStalyCzas("abc", "abd") && !serwer::rowneStalyCzas("ab", "abc"));
  // ikony przeglądarki bez licznika złych żądań
  serwer::Sesja ik;
  ik.otworz("k", 0, "192.168.1.47", serwer::Tryb::PDF, "/raporty/a.pdf", {}, DZIS, "T", "", false);
  for (int i = 0; i < 10; i++) SPRAWDZ(ik.obsluz(i % 2 ? "/favicon.ico" : "/apple-touch-icon-precomposed.png", 1).kod == 404);
  SPRAWDZ(ik.aktywna(1) && ik.zleZadania() == 0);
  // R-LEK-18: ściąga, kod prowadzi do EDF
  serwer::Sesja u;
  u.otworz("e", 0, "10.0.0.2", serwer::Tryb::SCIAGA, "", {}, DZIS, "T", "", false);
  ROWNE(u.adresQr(), "http://10.0.0.2/r/e/edf");
  SPRAWDZ(u.obsluz("/r/e/", 1).edf && u.obsluz("/r/e/raport.pdf", 1).kod == 404);
}

// ---------- konfiguracja ----------
static void testKonfiguracja() {
  SPRAWDZ(raport::wariantZKonfiguracji("") == raport::Wariant::PDF);
  SPRAWDZ(raport::wariantZKonfiguracji("# x\nraport=sciaga\n") == raport::Wariant::SCIAGA);
  SPRAWDZ(raport::wariantZKonfiguracji("raport=sciaga\r\nraport=pdf") == raport::Wariant::PDF);
  bool u = false;
  SPRAWDZ(raport::czynnikiZKonfiguracji("pylki=brzoza", u) == 0 && !u);
  SPRAWDZ(raport::czynnikiZKonfiguracji("raport.czynniki=A,K,N", u) == (CZ_ALKOHOL | CZ_KATAR) && u);
}

// ---------- treść stref według trybu (ADR-0030) ----------
static raport::Raport zbuduj(const std::vector<Noc>& h, const sprawy::SprawaL* t, const std::vector<sprawy::SprawaL>* sp = nullptr) {
  raport::Wejscie we;
  we.dzis = DZIS;
  we.czas = "06:04";
  we.historia = &h;
  we.temat = t;
  we.sprawy = sp;
  we.wersjaPanelu = "BETA";
  we.wersjaAirBridge = "2.3.0";
  return raport::zbuduj(we);
}
static void testTryby() {
  for (const char* tr : {"CPAP", "AutoSet", "ASV", "ASVAuto"}) {
    std::vector<Noc> h = historia(tr);
    sprawy::SprawaL s = sprawaL01();
    raport::Raport r = zbuduj(h, &s);
    std::vector<std::string> w = raport::wierszeTekstu(r);
    bool asv = std::string(tr).compare(0, 3, "ASV") == 0;
    SPRAWDZ(r.s1tytul.find(std::string(", tryb ") + tr) != std::string::npos);
    SPRAWDZ(zawiera(w, "EPAP mediana / 95%") == asv && zawiera(w, "Wsparcie PS mediana / 95%") == asv);
    SPRAWDZ(zawiera(w, "Ciśnienie mediana / 95%") == !asv);
    SPRAWDZ(zawiera(w, raport::ZDANIE_R_LEK_34) == asv);  // R-LEK-34
    SPRAWDZ(zawiera(w, raport::ZDANIE_R_LEK_28));          // R-LEK-28
    SPRAWDZ(zawiera(w, "Bez oksymetru: brak oceny natlenienia krwi."));
    SPRAWDZ(r.s5.size() == (asv ? 4u : 3u));               // AHI, EPAP i PS albo ciśnienie, wyciek
    ROWNE(r.s6[0].substr(0, std::string(tr).size()), tr);  // ustawienia teraz: opis stanu
    bool zmiana = std::string(tr) == "AutoSet" || std::string(tr) == "ASVAuto";  // syntetyczne: maksimum od nocy 45
    SPRAWDZ(r.s6zmiany.wiersze.size() == 1 && (r.s6zmiany.wiersze[0][1].find(" → ") != std::string::npos) == zmiana);
    if (!zmiana) ROWNE(r.s6zmiany.wiersze[0][1], "Bez zmian w okresie raportu.");
    // R-LEK-12: kolejność 1, 2, 4, 3, 3b, 5–10; w strefie 2 najpierw powód
    int i2 = indeks(w, "DLACZEGO TEN RAPORT"), i4 = indeks(w, "Bezdechy centralne (CA/h)"),
        i3 = indeks(w, "PYTANIE DO LEKARZA"), i5 = indeks(w, "KONTEKST, TE SAME 90 NOCY"), i9 = indeks(w, "CZEGO TE DANE");
    SPRAWDZ(i2 > 0 && i2 < i4 && i4 < i3 && i3 < i5 && i5 < i9);
    ROWNE(w[(size_t)i2 + 1], s.powod);
    ROWNE(r.s1pacjent, "Pacjent: ______________________  ur.: ____________");  // R-LEK-15
    std::string b;
    std::string p = raport::pdf(r, b);
    SPRAWDZ(b.empty() && p.size() > 1000 && p.size() <= raport::PDF_MAKS && p.compare(0, 8, "%PDF-1.4") == 0);
    SPRAWDZ(p.find("/Count 1") != std::string::npos);  // jedna strona
    zapisz(std::string("raport-L01-") + tr + ".pdf", p);
  }
  // okres 10 nocy ASVAuto po 20 nocach AutoSet: zdanie ASV jest (R-LEK-34)
  std::vector<Noc> h = historia("AutoSet", 30);
  for (int i = 20; i < 30; i++) h[(size_t)i].tryb = "ASVAuto";
  SPRAWDZ(zawiera(zbuduj(h, nullptr).s9, raport::ZDANIE_R_LEK_34));
}

// ---------- strefa 2: „tylko raport”, „nie oceniam”, wzorce C, objawy, czynniki, R-LEK-30 ----------
static void testStrefa2() {
  reguly::Zestaw z = atrapa_b5::zestaw();
  std::vector<reguly::Wynik> wy = {
      atrapa_b5::wynik(z, "L-11", "", true, true, {{"n", "2"}, {"x", "6,0"}}),
      atrapa_b5::wynik(z, "L-02", "a", true, true, {{"n", "4"}, {"min", "96"}}),  // „tylko raport”
      atrapa_b5::wynik(z, "L-07", "", true, true),                                  // „tylko raport” z samą linią C
      atrapa_b5::wynik(z, "L-08", "", true, true),                                  // C
      atrapa_b5::wynik(z, "L-04", "", false, false),                                // nie oceniam
      atrapa_b5::wynik(z, "L-03", "10", false, false),                              // nie oceniam
  };
  reguly::Pamiec pm;
  sprawy::FunkcjaTekstu f = atrapa_b5::funkcja(z);
  std::vector<sprawy::SprawaL> sp = sprawy::sprawyOtwarte(z, wy, pm, f);
  const sprawy::SprawaL* l11 = nullptr;
  for (auto& s : sp)
    if (s.id == "L-11") l11 = &s;
  SPRAWDZ(l11 != nullptr);
  std::vector<Noc> h = historia("ASVAuto");
  std::vector<sprawy::Odpowiedz> od = {{"2026-11-01", sprawy::Pytanie::SENNOSC, 't'},
                                       {"2026-10-25", sprawy::Pytanie::SENNOSC, 'n'},
                                       {"2026-10-18", sprawy::Pytanie::SENNOSC, 'b'},
                                       {"2026-11-03", sprawy::Pytanie::GORZEJ, 't'}};
  std::vector<WpisCzynnikow> cz(3);
  cz[0].data = "2026-10-30", cz[0].stan = 'O', cz[0].czynniki = CZ_KATAR;
  cz[1].data = "2026-11-02", cz[1].stan = 'O', cz[1].czynniki = CZ_KATAR | CZ_ALKOHOL;
  cz[2].data = "2026-10-01", cz[2].stan = 'O', cz[2].czynniki = CZ_KATAR;  // poza 14 nocami
  raport::Wejscie we;
  we.dzis = DZIS;
  we.historia = &h;
  we.temat = l11;
  we.sprawy = &sp;
  we.wyniki = &wy;
  we.pamiec = &pm;
  we.zestaw = &z;
  we.tekst = f;
  we.odpowiedzi = &od;
  we.czynniki = &cz;
  raport::Raport r = raport::zbuduj(we);
  ROWNE(r.s2.at(0), "Senność w 2 z 4 tygodni, urządzenie liczy 6,0 / h.");
  ROWNE(r.s2.at(1), std::string("„") + raport::PYTANIE_SENNOSC + "”: „tak” w 1 z 2 ostatnich tygodni.");
  ROWNE(r.s2.at(2), "Odpowiedź „gorzej”: tak, 3.11.");
  SPRAWDZ(zawiera(r.s2, "Zmiana ustawień: 21.09."));
  SPRAWDZ(zawiera(r.s2, "Oddychanie okresowe w 4 z 14 nocy, razem 96 min (bez oceny pilności)."));
  SPRAWDZ(zawiera(r.s2, "Ciśnienie rośnie od 2 tygodni (bez oceny pilności)."));  // linia C bez `Na kontroli: …`
  SPRAWDZ(zawiera(r.s2, "Regularność niższa od 4 tygodni."));
  SPRAWDZ(!zawiera(r.s2, "Na kontroli"));
  SPRAWDZ(zawiera(r.s2, "EPAP przy górnej granicy: nie oceniam."));  // „nie oceniam” jako „nie oceniam”
  SPRAWDZ(zawiera(r.s2, "Bezdechy utrzymują się wyżej: nie oceniam."));
  SPRAWDZ(!zawiera(r.s2, "Zgłoszone przez pacjenta"));  // L-11: czynniki domyślnie tylko przy L-01 (5.13.7)
  we.czynnikiUstawione = true;
  we.czynnikiWlaczone = CZ_KATAR | CZ_ALKOHOL;
  r = raport::zbuduj(we);
  SPRAWDZ(zawiera(r.s2, "Zgłoszone przez pacjenta: katar lub infekcja w 2 z 14 nocy."));
  SPRAWDZ(zawiera(r.s2, "Zgłoszone przez pacjenta: alkohol w 1 z 14 nocy."));
  // strefa 3: pytanie ogólne wzorca, gdy brak hipotezy (R-HIP-07); 3b z wyłączoną podpowiedzią znika
  ROWNE(r.s3pytanie, "Senność mimo terapii: czy to wymaga korekty leczenia albo dalszej diagnostyki?");
  SPRAWDZ(!r.s3bJest);  // bez hipotezy H nie ma strefy 3b (fail-path R-HIP-07)
  we.podpowiedziH = false;
  SPRAWDZ(!raport::zbuduj(we).s3bJest);

  // R-LEK-30 w raporcie ogólnym: zakończone, trwające A/B, trwające C z linią, limit 6
  reguly::Pamiec p2;
  auto wpis = [&](const std::string& k, wzorce::StanWzorca st, char pil, const std::string& od1, const std::string& do1) {
    wzorce::ZapisWzorca zw;
    zw.stan = st;
    zw.pil = pil;
    zw.wejscie = od1;
    zw.wyjscie = do1;
    p2[k].stanWzorca = wzorce::zapiszZapis(zw);
  };
  wpis("L-07", wzorce::StanWzorca::NIEAKTYWNY, 'C', "2026-08-03", "2026-09-24");
  wpis("L-08", wzorce::StanWzorca::AKTYWNY, 'C', "2026-10-02", "");
  wpis("L-03", wzorce::StanWzorca::AKTYWNY, 'B', "2026-10-05", "");
  wpis("L-05", wzorce::StanWzorca::NIEAKTYWNY, 'B', "2026-03-01", "2026-04-01");  // 150 nocy temu: bez zdania
  p2["L-03"].omowione = "2026-10-20@B";
  raport::Wejscie wo = we;
  wo.temat = nullptr;
  wo.pamiec = &p2;
  r = raport::zbuduj(wo);
  SPRAWDZ(zawiera(r.s2, "L-07: od 3.08 do 24.09.") || zawiera(r.s2, "Ciśnienie rośnie"));
  SPRAWDZ(zawiera(r.s2, "Regularność niższa od 4 tygodni. Od 2.10."));
  SPRAWDZ(!zawiera(r.s2, "od 2.10, trwa"));
  SPRAWDZ(zawiera(r.s2, "Bezdechy utrzymują się wyżej: od 5.10, trwa. Omówione z lekarzem: 20.10."));  // R-LEK-16
  SPRAWDZ(!zawiera(r.s2, "od 1.03"));
  for (int i = 0; i < 9; i++) wpis("L-9" + std::to_string(i), wzorce::StanWzorca::NIEAKTYWNY, 'B', "2026-09-01",
                                   dodajDni("2026-09-10", i));
  r = raport::zbuduj(wo);
  SPRAWDZ(zawiera(r.s2, "i 6 starszych (w kokpicie)"));  // 12 wzorców w 90 nocach: 6 najnowszych
  SPRAWDZ(indeks(r.s2, "Bezdechy utrzymują się wyżej: od 5.10, trwa.") < indeks(r.s2, "L-98: od 1.09 do 18.09."));
  ROWNE(r.temat, "Raport ogólny, 90 nocy");
}

// ---------- R-LEK-31, R-LEK-35, strefa 4 i 8 ----------
static void testLiczby() {
  std::vector<Noc> h = historia("AutoSet");
  sprawy::SprawaL s = sprawaL01();
  raport::Raport r = zbuduj(h, &s);
  // strefa 7: 30 nocy, w tym 1 krótka (1 h 30): noce ≥ 4 h 29 z 30 = 97%, kryterium spełnione
  ROWNE(r.s7.wiersze[0][0], "Noce ≥ 4 h");
  ROWNE(r.s7.wiersze[0][1], "97%");
  ROWNE(r.s7kryterium, "Kryterium 4 h w 70% nocy: spełnione");
  ROWNE(r.s7.wiersze[2][1], "9,8 / 12,4");
  ROWNE(r.s7dopisek, "Noce ≥ 4 h i średnia: wszystkie noce okresu. Pozostałe wiersze: noce z terapią. Mediany nie sumują się do AHI.");
  // 40 nocy w historii: kolumna 90 nocy z dopiskiem
  std::vector<Noc> h40 = historia("AutoSet", 40);
  SPRAWDZ(zbuduj(h40, nullptr).s7.wiersze[0][2].find("(z 40 nocy)") != std::string::npos);
  // R-LEK-35: CA/h z 7 nocy ≥ 5 → oznaczenie, AHI z 14 nocy < 10 → bez
  SPRAWDZ(r.s5zdania[1].find("· powyżej 5 / h") != std::string::npos);
  SPRAWDZ(r.s5zdania[2].find("powyżej") == std::string::npos);
  SPRAWDZ(r.s5zdania[0].compare(0, 10, "centralne ") == 0 && r.s5zdania[0].find("% AHI") != std::string::npos);
  // CA/h bez wartości (noce z karty) → `–`
  std::vector<Noc> hk = historia("ASVAuto");
  for (auto& n : hk) n.ca = NAN;
  SPRAWDZ(zbuduj(hk, nullptr).s5zdania[0].find("centralne – AHI (") == 0);
  // strefa 4: L-01 z linią 5 / h, początek wzorca, pasmo normy, × i puste kółko
  SPRAWDZ(r.s4.linie.size() == 1 && r.s4.linie[0] == 5 && r.s4.poczatek >= 0 && !std::isnan(r.s4.pasmoOd));
  int x = 0, o = 0;
  for (char c : r.s4.znak) {
    x += c == 'x';
    o += c == 'o';
  }
  SPRAWDZ(x == 5 && o == 1);
  ROWNE(r.s4tab.kolumny[1], "30 nocy przed");
  // strefa 8 wymienia wszystkie wyłączone noce
  ROWNE(r.s8[0], "Wyłączone z wyliczeń: 6 nocy. 1 × duży wyciek (P95 ≥ 15 L/min albo ≥ 10% czasu terapii albo ≥ 30 min powyżej 24 L/min "
                 "albo mediana > 30 L/min), 5 × nieważna (krócej niż 4 h, częściowa albo bez zdarzeń), 0 × brak danych "
                 "(noc bez terapii).");
  // R-LEK-19: metryka niewalidowana tylko jako trend
  raport::Wejscie we;
  we.dzis = DZIS;
  we.historia = &h;
  we.niewalidowane = {"M-02"};
  raport::Raport t = raport::zbuduj(we);
  // strefa 8 obiecuje paczkę EDF tylko wtedy, gdy panel ją ma
  SPRAWDZ(!t.s8.empty() && t.s8.back() == "Pełne przebiegi nocy: karta SD aparatu.");
  {
    raport::Wejscie z = we;
    z.edf = true;
    raport::Raport te = raport::zbuduj(z);
    SPRAWDZ(!te.s8.empty() && te.s8.back() == "Pełne przebiegi nocy: karta SD aparatu albo paczka EDF z panelu.");
  }
  ROWNE(t.s4tab.wiersze[0][1], "trend");
  SPRAWDZ(t.s7.wiersze[4][1] == "trend");
  // L-06 bez SpO2: `brak pomiaru` (fail-path R-LEK-13)
  sprawy::SprawaL l06 = s;
  l06.klucz = l06.id = "L-06";
  SPRAWDZ(zbuduj(h, &l06).s4.brakPomiaru);
}

// ---------- R-LEK-07 (procedura 9.3 w części tekstu) i brak danych ----------
static void testGranice() {
  std::vector<Noc> h = historia("ASVAuto");
  sprawy::SprawaL s = sprawaL01();
  for (auto& l : raport::wierszeTekstu(zbuduj(h, &s)))
    for (const char* zak : {"Ustaw", "Obniż", "Zwiększ", "Zmień", "menu", "/api/settings", "pilnie", "natychmiast", "112"})
      SPRAWDZ(l.find(zak) == std::string::npos);
  std::vector<Noc> pusta;
  raport::Raport r = zbuduj(pusta, nullptr);  // brak danych: treść z `–`, bez wyjątku
  ROWNE(r.s6[0], "–");
  ROWNE(r.s7.wiersze[2][1], "– / –");
  std::string b;
  SPRAWDZ(!raport::pdf(r, b).empty() && b.empty());
  // nazwy plików: kolejny raport tego dnia nie nadpisuje poprzedniego (R-LEK-14)
  raport::Raport t = zbuduj(h, &s);
  ROWNE(raport::nazwaPliku(t, DZIS, {}), "/raporty/2026-11-04-L-01.pdf");
  ROWNE(raport::nazwaPliku(t, DZIS, {"/raporty/2026-11-04-L-01.pdf"}), "/raporty/2026-11-04-L-01-2.pdf");
  ROWNE(raport::nazwaPliku(r, DZIS, {}), "/raporty/2026-11-04-ogolny.pdf");
}

// ---------- ekrany: QR i ściąga mieszczą się w 800×480, kod czytelny ----------
struct PlotnoTestu : Plotno {
  std::vector<uint8_t> b = std::vector<uint8_t>(800 * 480, 0);
  int poza = 0;
  void piksel(int x, int y, bool c) override {
    if (x < 0 || y < 0 || x >= 800 || y >= 480) {
      poza++;
      return;
    }
    b[(size_t)y * 800 + (size_t)x] = c;
  }
};
static void testEkrany() {
  PlotnoTestu p;
  DaneEkranuRaportu d;
  d.naglowek = {"śr 04.11 · raport gotowy", "Wi-Fi włączone"};
  d.temat = "Bezdechy centralne utrwalone";
  d.okres = "Okres: 07.08–04.11 · 1 strona A4";
  d.adresQr = "http://192.168.100.200/r/0123456789abcdefghjkmnpqrs/";
  d.adresReczny = "192.168.100.200/r/0123456789abcdefghjkmnpqrs";
  d.kodWazny = "Kod ważny do 08:21.";
  d.stan = "Pobrano o 07:42.";
  SPRAWDZ(rysujRaportQr(p, d) && p.poza == 0);
  SPRAWDZ(szer(F_P22B, "albo wpisz: " + d.adresReczny) <= 744);
  int akcje3 = szer(F_P22B, "◀ wróć") + 26 + szer(F_P22B, "▶ kokpit") + 26 + szer(F_P22B, "↻ 2 s starsze");
  SPRAWDZ(szer(F_P22B, d.stan) + 26 + akcje3 <= 744);
  // po ↻ 2 s: `Starsze raporty widoczne do …` z lewej, akcja `↻ 2 s starsze` znika (stopka się mieści)
  SPRAWDZ(szer(F_P22B, "Starsze raporty widoczne do 08:21.") + 26 + szer(F_P22B, "◀ wróć") + 26 +
              szer(F_P22B, "▶ kokpit") <= 744);
  // R-LEK-06: moduł ≥ 6 px, strefa ciszy ≥ 4 moduły wewnątrz białego pola
  PlotnoTestu q;
  int m = rysujKodQr(q, 0, 0, 302, d.adresQr);
  qr::Kod k = qr::koduj(d.adresQr);
  SPRAWDZ(m >= 6 && (302 - k.rozmiar * m) / 2 >= 4 * m);
  PlotnoTestu s;
  DaneSciagi c;
  c.naglowek = d.naglowek;
  c.temat = d.temat;
  c.pytanie = "Bezdechy centralne utrwalone: czy to wymaga korekty leczenia albo dalszej diagnostyki?";
  c.liczby[0] = "2,0";
  c.liczby[1] = "5,4";
  c.liczby[2] = "12,4";
  c.etykiety[0] = "30 nocy przed";
  c.etykiety[1] = "od początku wzorca";
  c.etykiety[2] = "ostatnie 7";
  c.podpis = "Centralne / h · okres 07.08–04.11.2026";
  c.adresQr = "http://192.168.1.47/r/0123456789abcdefghjkmnpqrs/edf";
  SPRAWDZ(rysujSciage(s, c) && s.poza == 0);
}

// ---------- dyrygent: ▶ na ekranie L otwiera raport, serwer 15 min, brak sieci ----------
class GniazdoAtrapy : public serwer::GniazdoHttp {
 public:
  bool startuje = true, slucha = false;
  int startow = 0, stopow = 0;
  std::vector<std::string> doWyslania;  // ścieżki żądań telefonu
  std::vector<serwer::Odpowiedz> odp;
  bool start(uint16_t) override {
    startow++;
    slucha = startuje;
    return startuje;
  }
  void stop() override {
    stopow++;
    slucha = false;
  }
  void obsluz(const std::function<serwer::Odpowiedz(const std::string&)>& f) override {
    if (!slucha || doWyslania.empty()) return;
    std::string s = doWyslania.front();
    doWyslania.erase(doWyslania.begin());
    odp.push_back(f(s));
  }
};

static uint32_t ep(int r, int mi, int d, int h, int m) {
  std::tm t{};
  t.tm_year = r - 1900, t.tm_mon = mi - 1, t.tm_mday = d, t.tm_hour = h, t.tm_min = m;
  t.tm_isdst = -1;
  return (uint32_t)mktime(&t);
}
static const KonfiguracjaPanelu KONF = {"192.168.1.20", "cpap", "haslo", "", "", true};
static const char* STATUS =
    "{\"version\":\"v2.2.1-25-gebbd145\",\"pna\":\"AirCurve_10_CS_PaceWave\",\"mode\":\"ASVAuto\",\"system\":\"IDLE\","
    "\"rop\":0,\"edf\":\"idle\",\"edf_prefix\":\"\",\"report_state\":\"idle\"}";
static std::string raportNocy(long day) {
  std::string d = std::to_string(day);
  return "{\"source\":\"edf\",\"revision\":154,\"data_revision\":11,\"view\":\"day\",\"day\":" + d +
         ",\"first_day\":" + d + ",\"last_day\":" + d + ",\"sessions\":[{\"id\":1,\"day\":" + d +
         ",\"prefix\":\"X\",\"start_ms\":1793403600000,\"end_ms\":1793428920000}],\"present\":true,\"partial\":false"
         ",\"used_seconds\":25320,\"rdi\":1.6,\"csr_seconds\":0,\"leak_threshold\":24,\"leak_high_seconds\":0,"
         "\"session_count\":1,\"ahi\":1.4,\"events_known\":true,\"counts\":{\"oa\":1,\"ca\":5,\"ua\":0,\"h\":4,"
         "\"rera\":2},\"channels\":[{\"id\":\"Leak\",\"label\":\"W\",\"unit\":\"L/min\",\"count\":12660,"
         "\"coverage_seconds\":25320,\"min\":0,\"mean\":3.1,\"median\":2.4,\"p95\":9.6,\"max\":30.2,"
         "\"source\":\"edf\"}],\"settings\":{\"day\":" + d +
         ",\"available\":true,\"fields\":[{\"tag\":\"MOP\",\"label\":\"Tryb\",\"value\":\"ASVAuto\",\"unit\":\"\","
         "\"option\":8}]}}";
}
static reguly::Zestaw ZESTAW_D = atrapa_b5::zestaw();
static std::vector<reguly::Wynik> wynikiD(const reguly::Kontekst&) {
  std::vector<reguly::Wynik> w;
  for (auto& k : ZESTAW_D.katalog)
    w.push_back(atrapa_b5::wynik(ZESTAW_D, k.id, k.wariant, k.stan != StanReguly::ZABLOKOWANA, k.id == "L-11",
                                 {{"x", "6,0"}, {"n", "2"}}));
  return w;
}
static void rano(Panel& p, int h, int m, PowodWybudzenia w) {
  p.zegar.epoch = ep(2026, 11, 4, h, m);
  p.zegar.msTeraz = 0;
  p.start.przyczynaStartu = PrzyczynaStartu::USPIENIE;
  p.start.powodWybudzenia = w;
  zerujPrzedSnem(p.start.l, identyfikatorWersji(p.start.napisWersji));
  long doba = nrDnia("2026-11-04") - 1;
  p.siec.dodaj("/api/status", 200, STATUS);
  p.siec.dodaj(sciezkaListy(), 200, "{\"source\":\"edf\",\"days\":[{\"day\":" + std::to_string(doba) +
                                         ",\"session_count\":1,\"stale\":false}]}");
  p.siec.dodaj(sciezkaNocy(doba), 200, raportNocy(doba));
}
static bool wLogu(const Panel& p, const std::string& f) {
  for (auto& l : p.port.log)
    if (l.find(f) != std::string::npos) return true;
  return false;
}
static void nacisnij(Panel& p, uint8_t bit, int h, int m, bool dlugo = false) {  // ▶ 2 s = raport z ekranu L
  p.port.log.clear();
  p.zegar.epoch = ep(2026, 11, 4, h, m);
  p.zegar.msTeraz = 0;
  p.start.powodWybudzenia = PowodWybudzenia::PRZYCISK;
  p.start.budzace = bit;
  p.przyciski.skrypt = {{0, bit}, {dlugo ? 2300 : 300, 0}};
  zerujPrzedSnem(p.start.l, identyfikatorWersji(p.start.napisWersji));
  Dyrygent(p.adaptery(), KONF).wybudzenie();
}
static void testDyrygent() {
  sprawy::zrodloTestowe.wyniki = wynikiD;
  sprawy::zrodloTestowe.zestaw = &ZESTAW_D;
  sprawy::zrodloTestowe.tekst = atrapa_b5::funkcja(ZESTAW_D);
  GniazdoAtrapy g;
  serwer::ustawGniazdo(&g);
  {  // ▶ na ekranie L-11: PDF w /raporty/, serwer, strona i PDF dla telefonu, po 15 min ekran L
    Panel p;
    rano(p, 7, 0, PowodWybudzenia::ZEGAR);
    Dyrygent(p.adaptery(), KONF).wybudzenie();
    SPRAWDZ(wLogu(p, "ekran dnia: wzorzec L-11"));
    g.doWyslania = {"/", "/r/zly/", "PODMIEN/", "PODMIEN/raport.pdf", "PODMIEN/edf"};
    // token z atrapy generatora (stała 0x12345678): 16 bajtów 78 56 34 12 × 4
    uint8_t b[16];
    for (int i = 0; i < 16; i++) b[i] = (uint8_t)(0x12345678u >> (8 * (i % 4)));
    std::string tok = serwer::token(b);
    for (auto& s : g.doWyslania)
      if (s.compare(0, 7, "PODMIEN") == 0) s = "/r/" + tok + s.substr(7);
    nacisnij(p, BIT_PRAWY, 7, 10, true);
    SPRAWDZ(wLogu(p, "raport dla lekarza: /raporty/2026-11-04-L-11.pdf"));
    SPRAWDZ(p.pliki.pliki.count("/raporty/2026-11-04-L-11.pdf") == 1);
    SPRAWDZ(p.pliki.pliki["/raporty/2026-11-04-L-11.pdf"].compare(0, 8, "%PDF-1.4") == 0);
    SPRAWDZ(wLogu(p, "raport dla lekarza: serwer do 07:25"));
    SPRAWDZ(g.odp.size() == 5 && g.odp[0].kod == 404 && g.odp[1].kod == 404 && g.odp[2].kod == 200);
    SPRAWDZ(g.odp.size() == 5 && g.odp[3].kod == 200 && g.odp[3].cialo.compare(0, 8, "%PDF-1.4") == 0);
    // `/edf` bez paczki mówi wprost, że jej nie ma; nigdy „AirBridge nie odpowiada”
    SPRAWDZ(g.odp.size() == 5 && g.odp[4].kod == 501 && g.odp[4].cialo == serwer::EDF_NIEDOSTEPNA);
    SPRAWDZ(g.odp.size() == 5 && g.odp[2].cialo.find("AirBridge nie odpowiada") == std::string::npos);
    SPRAWDZ(wLogu(p, "raport dla lekarza: minęło 15 min") && wLogu(p, "serwer wyłączony"));
    SPRAWDZ(p.start.stan.ekran == (uint8_t)Ekran::DZIEN);  // R-LEK-04: wraca do ekranu, z którego otwarto
    SPRAWDZ(!g.slucha && g.stopow >= 1);
    if (g.odp.size() == 5) zapisz("strona-telefonu.html", g.odp[2].cialo);
    // ponowne otwarcie tego dnia: nowy plik, stary zostaje (R-LEK-14)
    g.odp.clear();
    nacisnij(p, BIT_PRAWY, 7, 40, true);
    SPRAWDZ(p.pliki.pliki.count("/raporty/2026-11-04-L-11-2.pdf") == 1);
  }
  {  // brak sieci: `Nie mogę włączyć Wi-Fi`, raport w pamięci panelu, serwer nie startuje (R-LEK-22)
    Panel p;
    rano(p, 7, 0, PowodWybudzenia::ZEGAR);
    Dyrygent(p.adaptery(), KONF).wybudzenie();
    p.siec.siecJest = false;
    int startow = g.startow;
    nacisnij(p, BIT_PRAWY, 7, 10, true);
    SPRAWDZ(wLogu(p, "nie mogę włączyć Wi-Fi"));
    SPRAWDZ(p.pliki.pliki.count("/raporty/2026-11-04-L-11.pdf") == 1);
    SPRAWDZ(g.startow == startow);
    SPRAWDZ(p.start.stan.ekran == (uint8_t)Ekran::RAPORT);
  }
  {  // ▶ 2 s w K2: raport ogólny; ◀ na ekranie raportu wraca do K2 i wyłącza serwer (R-LEK-11)
    Panel p;
    rano(p, 7, 0, PowodWybudzenia::ZEGAR);
    Dyrygent(p.adaptery(), KONF).wybudzenie();
    p.start.stan.ekran = (uint8_t)Ekran::K1;  // ekran dnia to ekran L (▶ = raport tematyczny), więc K1 wprost
    nacisnij(p, BIT_PRAWY, 7, 5);  // K1 ▶ → K2
    SPRAWDZ(p.start.stan.ekran == (uint8_t)Ekran::K2);
    p.port.log.clear();
    p.zegar.epoch = ep(2026, 11, 4, 7, 6);
    p.zegar.msTeraz = 0;
    p.start.powodWybudzenia = PowodWybudzenia::PRZYCISK;
    p.start.budzace = BIT_PRAWY;
    p.przyciski.skrypt = {{0, BIT_PRAWY}, {2500, 0}, {7000, BIT_LEWY}, {7300, 0}};
    zerujPrzedSnem(p.start.l, identyfikatorWersji(p.start.napisWersji));
    Dyrygent(p.adaptery(), KONF).wybudzenie();
    SPRAWDZ(wLogu(p, "raport dla lekarza: /raporty/2026-11-04-ogolny.pdf"));
    SPRAWDZ(wLogu(p, "serwer wyłączony") && !wLogu(p, "minęło 15 min"));
    SPRAWDZ(p.start.stan.ekran == (uint8_t)Ekran::K2);
  }
  {  // wariant ściąga z konfiguracji: bez PDF, serwer z kodem do EDF
    Panel p;
    rano(p, 7, 0, PowodWybudzenia::ZEGAR);
    p.pliki.pliki["/konfiguracja.txt"] = "raport=sciaga\n";
    Dyrygent(p.adaptery(), KONF).wybudzenie();
    int startow = g.startow;
    nacisnij(p, BIT_PRAWY, 7, 10, true);
    SPRAWDZ(p.pliki.pliki.count("/raporty/2026-11-04-L-11.pdf") == 0);
    // bez paczki EDF ściąga bez Wi-Fi, serwera i kodu QR
    SPRAWDZ(wLogu(p, "ściąga bez paczki EDF, bez Wi-Fi i serwera") && !wLogu(p, "(ściąga)"));
    SPRAWDZ(g.startow == startow && !g.slucha);
    SPRAWDZ(p.start.stan.ekran == (uint8_t)Ekran::RAPORT);
  }
  {  // brak gniazda: jak brak sieci
    serwer::ustawGniazdo(nullptr);
    Panel p;
    rano(p, 7, 0, PowodWybudzenia::ZEGAR);
    Dyrygent(p.adaptery(), KONF).wybudzenie();
    nacisnij(p, BIT_PRAWY, 7, 10, true);
    SPRAWDZ(wLogu(p, "nie mogę włączyć Wi-Fi"));
  }
  serwer::ustawGniazdo(nullptr);
  sprawy::zrodloTestowe = sprawy::ZrodloWynikow();
}

int main(int argc, char** argv) {
  setenv("TZ", "Europe/Warsaw", 1);
  tzset();
  if (argc > 1) KATALOG = argv[1];
  std::string polecenie = "mkdir -p '" + KATALOG + "'";
  if (system(polecenie.c_str()) != 0) printf("uwaga: nie mogę utworzyć %s\n", KATALOG.c_str());
  testQr();
  testToken();
  testSerwer();
  testKonfiguracja();
  testTryby();
  testStrefa2();
  testLiczby();
  testGranice();
  testEkrany();
  testDyrygent();
  printf("testy_raport_a4: %d ok, %d błędów (przykładowe PDF i strona telefonu: %s)\n", ok, zle, KATALOG.c_str());
  return zle ? 1 : 0;
}
