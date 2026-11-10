// SPDX-License-Identifier: GPL-3.0-or-later
// Konfiguracja z telefonu przez kod QR, testy na atrapach. Sekrety poza obrazem firmware
// (src/sekrety.h): NVS w przestrzeni `sekrety`, secrets.h tylko jako wartość początkowa, stan „pierwsza
// konfiguracja” bez danych, panel po aktualizacji działa dalej bez formularza, hasła nigdy w logu,
// zrzucie plików `Z`, na U-05 ani w plikach (R-KONF-38). Dane syntetyczne.
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <deque>
#include <string>
#include <vector>

#include "adaptery_atrapy.h"
#include "airbridge.h"
#include "dyrygent.h"
#include "ekrany_konfiguracji.h"
#include "formularz.h"
#include "nawigacja.h"
#include "serwer_raportu.h"
#include "sekrety.h"
#include "start.h"
#include "wspolne.h"

static int ok = 0, zle = 0;
#define SPRAWDZ(w)                                                       \
  do {                                                                   \
    if (w) ok++;                                                         \
    else { zle++; printf("BŁĄD %s:%d: %s\n", __FILE__, __LINE__, #w); } \
  } while (0)

static const char* const HASLO_WIFI = "Wifi-Tajne-7731";
static const char* const HASLO_AB = "Ab-Tajne-9921";

static sekrety::Dane daneKompilacji() {
  sekrety::Dane d;
  d.ssid = "Dom";
  d.hasloWifi = HASLO_WIFI;
  d.abAdres = "192.168.1.20";
  d.abKonto = "admin";
  d.abHaslo = HASLO_AB;
  return d;
}

// ---------- etap 1: logika wczytania ----------
static void sekretyLogika() {
  Dziennik dz;
  {  // NVS pusty, secrets.h z danymi → kopia do NVS (źródło „k”)
    AtrapaNvs n(dz);
    sekrety::Wynik w = sekrety::wczytaj(n, daneKompilacji());
    SPRAWDZ(!w.pierwszaKonfiguracja() && w.zrodlo == sekrety::Zrodlo::KOMPILACJA && w.zapisanoPoczatkowe);
    SPRAWDZ(n.teksty["sekrety/ssid"] == "Dom" && n.teksty["sekrety/wifi"] == HASLO_WIFI);
    SPRAWDZ(n.teksty["sekrety/ab"] == "192.168.1.20" && n.teksty["sekrety/ab_h"] == HASLO_AB);
    SPRAWDZ(n.teksty["sekrety/zrodlo"] == "k");
    // drugi start z tymi samymi secrets.h: z NVS, bez ponownego zapisu
    int przed = n.zapisy;
    sekrety::Wynik w2 = sekrety::wczytaj(n, daneKompilacji());
    SPRAWDZ(w2.d.ssid == "Dom" && !w2.zapisanoPoczatkowe && n.zapisy == przed);
    // aktualizacja bez sekretów w secrets.h (obraz bez sieci): panel działa dalej z NVS
    sekrety::Wynik w3 = sekrety::wczytaj(n, sekrety::Dane());
    SPRAWDZ(!w3.pierwszaKonfiguracja() && w3.d.abHaslo == HASLO_AB && w3.d.hasloWifi == HASLO_WIFI);
    // nowe secrets.h (np. egzemplarz testowy wgrany z jego siecią): kopia od nowa, bo NVS pochodzi z kompilacji
    sekrety::Dane inne = daneKompilacji();
    inne.ssid = "Tester";
    sekrety::Wynik w4 = sekrety::wczytaj(n, inne);
    SPRAWDZ(w4.d.ssid == "Tester" && w4.zapisanoPoczatkowe && n.teksty["sekrety/ssid"] == "Tester");
  }
  {  // NVS pusty, secrets.h bez sieci → pierwsza konfiguracja, nic nie zapisane
    AtrapaNvs n(dz);
    sekrety::Dane bezSieci = daneKompilacji();
    bezSieci.ssid.clear();
    sekrety::Wynik w = sekrety::wczytaj(n, bezSieci);
    SPRAWDZ(w.pierwszaKonfiguracja() && n.zapisy == 0);
    sekrety::Dane bezAdresu = daneKompilacji();
    bezAdresu.abAdres.clear();
    SPRAWDZ(sekrety::wczytaj(n, bezAdresu).pierwszaKonfiguracja() && n.zapisy == 0);
    SPRAWDZ(sekrety::wczytaj(n, sekrety::Dane()).pierwszaKonfiguracja());
    SPRAWDZ(sekrety::opis(w).find("pierwsza konfiguracja") != std::string::npos);
  }
  {  // zapis z formularza wygrywa nad secrets.h (także nowymi)
    AtrapaNvs n(dz);
    sekrety::wczytaj(n, daneKompilacji());
    sekrety::Dane f = daneKompilacji();
    f.ssid = "Nowa";
    f.abHaslo = "";  // AirBridge bez logowania
    SPRAWDZ(sekrety::zapiszZFormularza(n, f));
    sekrety::Dane inne = daneKompilacji();
    inne.ssid = "Inna";
    sekrety::Wynik w = sekrety::wczytaj(n, inne);
    SPRAWDZ(w.zrodlo == sekrety::Zrodlo::FORMULARZ && w.d.ssid == "Nowa" && w.d.abHaslo.empty());
    SPRAWDZ(!sekrety::zapiszZFormularza(n, sekrety::Dane()));  // niekompletne: nic
    SPRAWDZ(n.teksty["sekrety/ssid"] == "Nowa");
  }
  {  // zapis przerwany w połowie („z”): bez mieszanki, z secrets.h od nowa; bez secrets.h pierwsza konfiguracja
    AtrapaNvs n(dz);
    n.teksty["sekrety/zrodlo"] = "z";
    n.teksty["sekrety/ssid"] = "Polowa";
    SPRAWDZ(sekrety::wczytaj(n, sekrety::Dane()).pierwszaKonfiguracja());
    sekrety::Wynik w = sekrety::wczytaj(n, daneKompilacji());
    SPRAWDZ(w.d.ssid == "Dom" && n.teksty["sekrety/zrodlo"] == "k");
  }
  {  // opis do logu bez haseł
    AtrapaNvs n(dz);
    std::string o = sekrety::opis(sekrety::wczytaj(n, daneKompilacji()));
    SPRAWDZ(o.find(HASLO_WIFI) == std::string::npos && o.find(HASLO_AB) == std::string::npos);
    SPRAWDZ(o.find("hasło AirBridge ustawione") != std::string::npos && o.find("sieć Dom") != std::string::npos);
  }
}

// ---------- etap 1: panel po aktualizacji (dyrygent na atrapach) i ochrona haseł ----------
static std::string status() {
  return "{\"version\":\"v2.3.0\",\"pna\":\"AirCurve_10_ASV\",\"srn\":\"1\",\"mode\":\"ASVAuto\",\"system\":\"IDLE\","
         "\"rop\":0,\"edf\":\"idle\",\"edf_prefix\":\"\",\"report_state\":\"idle\"}";
}
static KonfiguracjaPanelu konfZ(const sekrety::Wynik& s) {
  KonfiguracjaPanelu k;
  k.abAdres = s.d.abAdres;
  k.abKonto = s.d.abKonto;
  k.abHaslo = s.d.abHaslo;
  k.oknoZHaslem = true;
  k.pierwszaKonfiguracja = s.pierwszaKonfiguracja();
  k.opisSekretow = sekrety::opis(s);
  return k;
}
static bool maHaslo(const std::string& t) {
  return t.find(HASLO_WIFI) != std::string::npos || t.find(HASLO_AB) != std::string::npos;
}

static void panelWlasciciela() {
  Panel p;
  // start 1: panel z danymi sprzed (NVS bez przestrzeni `sekrety`), obraz z secrets.h
  sekrety::Wynik s = sekrety::wczytaj(p.nvs, daneKompilacji());
  p.siec.dodaj("/api/status", 401, "");
  p.siec.odpowiedzi["/api/status"].front().o.wwwAuthenticate =
      "Digest realm=\"AirBridge\", nonce=\"abc\", qop=\"auth\", algorithm=MD5";
  p.siec.dodaj("/api/status", 200, status());
  p.siec.dodaj(sciezkaListy(), 200, "{\"source\":\"edf\",\"days\":[]}");
  p.zegar.epoch = 1793948400;  // 2026-11-06 07:00 UTC
  p.start.przyczynaStartu = PrzyczynaStartu::ZASILANIE;
  p.start.powodWybudzenia = PowodWybudzenia::START;
  zerujPrzedSnem(p.start.l, identyfikatorWersji(p.start.napisWersji));
  Dyrygent dy(p.adaptery(), konfZ(s));
  dy.wybudzenie();
  bool status192 = false, digest = false;
  for (size_t i = 0; i < p.siec.zadania.size(); i++) {
    if (p.siec.zadania[i].find("http://192.168.1.20/api/status") == 0) status192 = true;
    for (auto& h : p.siec.naglowki[i]) {
      if (h.first == "Authorization" && h.second.find("username=\"admin\"") != std::string::npos) digest = true;
      SPRAWDZ(!maHaslo(h.second));  // Digest: skrót, nie hasło
    }
  }
  SPRAWDZ(status192 && digest);
  // start 2: nowy obraz bez sieci w secrets.h (np. dla wszystkich): panel dalej łączy się z AirBridge z NVS
  sekrety::Wynik s2 = sekrety::wczytaj(p.nvs, sekrety::Dane());
  SPRAWDZ(!s2.pierwszaKonfiguracja() && s2.d.abHaslo == HASLO_AB);
  p.siec.zadania.clear();
  p.siec.odpowiedzi.clear();
  p.siec.dodaj("/api/status", 200, status());
  p.siec.dodaj(sciezkaListy(), 200, "{\"source\":\"edf\",\"days\":[]}");
  p.port.znaki = {'Z', 'O'};  // zrzut plików `Z` i U-05 z oknem serwisowym w czuwaniu (okno przywracania już było)
  p.zegar.msTeraz = 0;
  zerujPrzedSnem(p.start.l, identyfikatorWersji(p.start.napisWersji));
  Dyrygent dy2(p.adaptery(), konfZ(s2));
  dy2.wybudzenie();
  for (auto& l : dy2.teksty().linieTestu) SPRAWDZ(!maHaslo(l));
  for (auto& l : dy2.teksty().diagnostykaTestu) SPRAWDZ(!maHaslo(l));
  bool logKonf = false, u05 = false, zrzut = false;
  for (auto& l : p.port.log) {
    SPRAWDZ(!maHaslo(l));
    if (l.find("konfiguracja: sieć Dom, AirBridge 192.168.1.20") != std::string::npos) logKonf = true;
    if (l.find("U-05 · ") != std::string::npos) u05 = true;
    if (l.find("PLIKI_START") != std::string::npos) zrzut = true;
  }
  SPRAWDZ(logKonf);
  SPRAWDZ(u05);
  SPRAWDZ(zrzut);
  if (getenv("KONFIGURACJA_LOG")) for (auto& l : p.port.log) printf("  | %s\n", l.c_str());
  for (auto& l : p.d.z) SPRAWDZ(!maHaslo(l));
  for (auto& kv : p.pliki.pliki) SPRAWDZ(!maHaslo(kv.second));
  for (auto& l : dy.teksty().linieTestu) SPRAWDZ(!maHaslo(l));
  for (auto& l : dy.teksty().diagnostykaTestu) SPRAWDZ(!maHaslo(l));
  // hasła tylko w przestrzeni `sekrety`
  for (auto& kv : p.nvs.teksty)
    if (kv.first.compare(0, 8, "sekrety/") != 0) SPRAWDZ(!maHaslo(kv.second));
  for (auto& kv : p.nvs.bajtyMap) SPRAWDZ(!maHaslo(kv.second));

  bool znow = false;
  for (auto& u : p.siec.zadania)
    if (u.find("http://192.168.1.20/api/status") == 0) znow = true;
  SPRAWDZ(znow);
}


// ---------- etap 2: formularz (rdzeń) ----------
static bool zawiera(const std::string& t, const std::string& x) { return t.find(x) != std::string::npos; }

static void formularzStrona() {
  sekrety::Dane d = daneKompilacji();
  d.ssid = "<script>alert(1)</script>\"'&";
  formularz::Pola p = formularz::zPliku("profil=publiczny\nwymiana.filtr=2026-09-10\nokres.rura=10\nokres.filtr_tyg=6\n", d);
  // formularz ma tylko maskę (0) i filtr (1, okres w tygodniach); klucze rury są ignorowane
  SPRAWDZ(p.profil == "publiczny" && p.wymiana[1] == "2026-09-10" && p.okres[1] == "6" && p.hasloWifi && p.hasloAb);
  std::string h = formularz::strona(p, "/u/abc/", "Zapisano o 21:14.", {{"ab_adres", "Zły <adres>"}});
  SPRAWDZ(!maHaslo(h));                               // R-KONF-38: hasła nigdy w stronie
  SPRAWDZ(!zawiera(h, "<script>alert"));              // XSS przez nazwę sieci
  SPRAWDZ(zawiera(h, "&lt;script&gt;alert(1)&lt;/script&gt;&quot;&#39;&amp;"));
  SPRAWDZ(zawiera(h, "Zły &lt;adres&gt;"));
  SPRAWDZ(zawiera(h, "placeholder=\"ustawione\""));
  SPRAWDZ(!zawiera(h, "http://") && !zawiera(h, "https://") && !zawiera(h, "src=") && !zawiera(h, "//cdn"));
  SPRAWDZ(zawiera(h, "Te ustawienia dotyczą tylko panelu, nie aparatu."));
  SPRAWDZ(zawiera(h, "Panel nie ostrzega o nagłych stanach. Przy duszności lub bólu w klatce wezwij pomoc."));
  SPRAWDZ(zawiera(h, "method=\"post\" action=\"/u/abc/\""));
  SPRAWDZ(zawiera(h, "value=\"publiczny\" selected"));
  SPRAWDZ(h.size() < 12000);  // pamięć serwera panelu
}

static void formularzCialo() {
  auto m = formularz::parsujCialo("ssid=M%C3%B3j+dom&wifi_haslo=a%26b%3Dc%25&x=&zly=%G1&ab_bez=1&ssid=Dom");
  SPRAWDZ(m["ssid"] == "Dom" && m["wifi_haslo"] == "a&b=c%" && m["x"].empty() && m["zly"] == "%G1");
  SPRAWDZ(formularz::parsujCialo("a=" + std::string(formularz::MAKS_CIALA, 'x')).empty());
  SPRAWDZ(formularz::parsujCialo(formularz::koduj("ż ółw&=%+") + "=" + formularz::koduj("A b&c=+%"))["ż ółw&=%+"] == "A b&c=+%");
}

static void formularzWalidacja() {
  sekrety::Dane ob = daneKompilacji();
  const std::string dzis = "2026-11-05";
  {  // puste hasła przy tej samej sieci: bez zmiany (R-KONF-39)
    formularz::Wynik w = formularz::waliduj({{"ssid", "Dom"}, {"wifi_haslo", ""}, {"ab_adres", "192.168.1.20"},
                                             {"ab_konto", "admin"}, {"ab_haslo", ""}}, ob, dzis);
    SPRAWDZ(w.ok() && !w.zmianaSieci && !w.zmianaAirBridge && w.sekrety.hasloWifi == HASLO_WIFI && w.sekrety.abHaslo == HASLO_AB);
  }
  {  // `bez hasła` usuwa hasło AirBridge
    formularz::Wynik w = formularz::waliduj({{"ab_haslo", ""}, {"ab_bez", "1"}}, ob, dzis);
    SPRAWDZ(w.ok() && w.zmianaAirBridge && w.sekrety.abHaslo.empty());
  }
  {  // nowa sieć bez hasła i bez `bez hasła`: błąd przy polu, sekrety bez zmian
    formularz::Wynik w = formularz::waliduj({{"ssid", "Nowa"}, {"wifi_haslo", ""}}, ob, dzis);
    SPRAWDZ(!w.ok() && w.bledy.count("wifi_haslo") && w.sekrety.ssid == "Dom" && !w.zmianaSieci);
    formularz::Wynik w2 = formularz::waliduj({{"ssid", "Nowa"}, {"wifi_haslo", "nowehaslo1"}}, ob, dzis);
    SPRAWDZ(w2.ok() && w2.zmianaSieci && w2.sekrety.ssid == "Nowa" && w2.sekrety.hasloWifi == "nowehaslo1");
    SPRAWDZ(formularz::waliduj({{"ssid", "Nowa"}, {"wifi_haslo", "krotkie"}}, ob, dzis).bledy.count("wifi_haslo"));
    SPRAWDZ(formularz::waliduj({{"ssid", ""}}, ob, dzis).bledy.count("ssid"));
    SPRAWDZ(formularz::waliduj({{"ssid", std::string(33, 'a')}}, ob, dzis).bledy.count("ssid"));
  }
  {  // adres AirBridge tylko w sieci lokalnej (R-NF-27, R-KONF-40)
    for (const char* a : {"192.168.1.50", "10.0.0.2:8080", "airbridge.local", "AirBridge.local:81", "172.16.4.4"})
      SPRAWDZ(formularz::adresAirBridgeOk(a));
    for (const char* a : {"", "8.8.8.8", "example.com", "192.168.1.50:0", "192.168.1.50:70000", "http://192.168.1.5",
                          ".local", "a b.local", "192.168.1.50/x", "airbridge.local:", "-x.local"})
      SPRAWDZ(!formularz::adresAirBridgeOk(a));
    formularz::Wynik w = formularz::waliduj({{"ab_adres", "8.8.8.8"}, {"profil", "publiczny"}}, ob, dzis);
    SPRAWDZ(w.bledy.count("ab_adres") && w.sekrety.abAdres == "192.168.1.20" && w.plik["profil"] == "publiczny");
  }
  {  // pozostałe pola (R-KONF-18: zła wartość odrzucona, reszta zapisana; R-KONF-34: bez dat z przyszłości)
    formularz::Wynik w = formularz::waliduj({{"wymiana.filtr", "2026-12-01"}, {"okres.filtr_tyg", "0"}, {"okres.maska", "12"},
                                             {"wymiana.maska", "2026-10-01"}, {"wymiana.rura", ""}, {"profil", "x"},
                                             {"poduszka", "pianka"}, {"okres.rura", "12"}, {"cisnienie", "12"}}, ob, dzis);
    SPRAWDZ(w.bledy.count("wymiana.filtr") && w.bledy.count("okres.filtr_tyg") && w.bledy.count("profil"));
    // rura, komora i typ poduszki nie są polami formularza (ciało ignorowane, bez błędu); stary wiersz
    // `wymiana.poduszka` znika przy zapisie daty maski (data scalona z maską)
    SPRAWDZ(w.plik["okres.maska"] == "12" && w.plik["wymiana.maska"] == "2026-10-01" && w.plik.count("wymiana.poduszka") &&
            w.plik["wymiana.poduszka"].empty() && !w.plik.count("wymiana.rura") && !w.plik.count("okres.rura") &&
            !w.plik.count("poduszka") && !w.bledy.count("poduszka") && !w.bledy.count("okres.rura"));
    SPRAWDZ(!w.plik.count("cisnienie") && !w.plik.count("wymiana.filtr"));  // R-KONF-01: tylko klucze z 5.6.4
  }
  {  // scalenie z plikiem: reszta wierszy bez zmian
    std::string st = "# komentarz\nco2=nie\nprofil=prywatny\nwymiana.rura=2026-01-01\nprofil=publiczny\nraport=sciaga";
    std::string n = formularz::scal(st, {{"profil", "publiczny"}, {"wymiana.rura", ""}, {"okres.filtr", "3"}});
    SPRAWDZ(n == "# komentarz\nco2=nie\nprofil=publiczny\nraport=sciaga\nokres.filtr=3\n");
    SPRAWDZ(formularz::scal("", {{"wymiana.rura", ""}}).empty());
  }
}

static void formularzSesja() {
  formularz::Sesja s;
  const std::string t = "f1b384krart14y2p6g97gnhm28";
  s.otworz(t, 1000, "192.168.1.47");
  SPRAWDZ(s.adresQr() == "http://192.168.1.47/u/" + t + "/" && s.adresReczny() == "192.168.1.47/u/" + t);
  SPRAWDZ(s.obsluz("GET", "/u/" + t + "/", "", 2000, "STRONA").cialo == "STRONA");
  SPRAWDZ(s.obsluz("GET", "/u/" + t, "", 2000, "STRONA").kod == 200);
  formularz::Odpowiedz o = s.obsluz("POST", "/u/" + t + "/", "profil=publiczny", 2000, "");
  SPRAWDZ(o.zapis && o.pola["profil"] == "publiczny");
  SPRAWDZ(s.obsluz("GET", "/favicon.ico", "", 2000, "").kod == 404 && s.zleZadania() == 0);
  SPRAWDZ(s.obsluz("GET", "/u/" + t + "/x", "", 2000, "S").kod == 404 && s.zleZadania() == 1);
  SPRAWDZ(s.obsluz("PUT", "/u/" + t + "/", "", 2000, "").kod == 405);
  for (int i = 0; i < 4; i++) SPRAWDZ(s.obsluz("GET", "/u/zly/", "", 2000, "S").kod == 404);
  SPRAWDZ(!s.aktywna(2000) && s.zamknietaZlymi());  // R-KONF-14
  SPRAWDZ(s.obsluz("GET", "/u/" + t + "/", "", 2000, "S").kod == 404);
  s.otworz(t, 1000, "192.168.1.47");
  SPRAWDZ(s.aktywna(1000 + serwer::CZAS_MS - 1) && !s.aktywna(1000 + serwer::CZAS_MS));  // R-KONF-15
  formularz::Sesja pusta;
  pusta.otworz("", 0, "192.168.1.47");
  SPRAWDZ(!pusta.aktywna(1) && pusta.obsluz("GET", "/u//", "", 1, "S").kod == 404);
}

// ---------- etap 2: mapa przycisków U-01, U-03 ----------
static void mapaUstawien() {
  KontekstNaw k;
  // U-01 to strona 6 / 6 kokpitu (po „Sprzęt”), dalej ciąg U-01 ▶ U-03 ▶ U-05
  k.kursorSprzetu = 1;  // ostatni wiersz „Sprzęt”
  SPRAWDZ(nacisniecie(Ekran::SPRZET, Przycisk::PRAWY, false, k).ekran == Ekran::USTAWIENIA);  // R-KONF-02
  k = KontekstNaw();
  SPRAWDZ(nacisniecie(Ekran::USTAWIENIA, Przycisk::SRODEK, false, k).ekran == Ekran::DZIEN);
  SPRAWDZ(nacisniecie(Ekran::USTAWIENIA, Przycisk::SRODEK, true, k).akcja == Akcja::AKT_SPRAWDZ);  // R-KONF-43: U-07 (aktualizacja z GitHub)
  SPRAWDZ(nacisniecie(Ekran::USTAWIENIA, Przycisk::LEWY, false, k).ekran == Ekran::SPRZET);   // R-KONF-33
  // ◀ 2 s na U-01 = sieć panelu (było „nic”)
  SPRAWDZ(nacisniecie(Ekran::USTAWIENIA, Przycisk::LEWY, true, k).akcja == Akcja::SIEC_PANELU_START);
  // Wi-Fi i serwer formularza (U-03) oraz okno serwisowe (U-05) tylko przytrzymaniem ▶, krótkie ▶ nic
  SPRAWDZ(nacisniecie(Ekran::USTAWIENIA, Przycisk::PRAWY, true, k).ekran == Ekran::FORMULARZ);  // R-KONF-12
  SPRAWDZ(nacisniecie(Ekran::USTAWIENIA, Przycisk::PRAWY, false, k).akcja == Akcja::NIC);
  SPRAWDZ(nacisniecie(Ekran::FORMULARZ, Przycisk::PRAWY, true, k).ekran == Ekran::TEST);  // R-KONF-36
  SPRAWDZ(nacisniecie(Ekran::FORMULARZ, Przycisk::PRAWY, false, k).akcja == Akcja::NIC);
  SPRAWDZ(nacisniecie(Ekran::TEST, Przycisk::LEWY, false, k).ekran == Ekran::USTAWIENIA);
  SPRAWDZ(nacisniecie(Ekran::FORMULARZ, Przycisk::LEWY, false, k).ekran == Ekran::USTAWIENIA);  // R-KONF-16
  SPRAWDZ(nacisniecie(Ekran::FORMULARZ, Przycisk::SRODEK, false, k).ekran == Ekran::DZIEN);
  for (Przycisk p : {Przycisk::SRODEK, Przycisk::LEWY})
    SPRAWDZ(nacisniecie(Ekran::FORMULARZ, p, true, k).akcja == Akcja::NIC &&
            nacisniecie(Ekran::FORMULARZ, p, true, k).ekran == Ekran::FORMULARZ);
  // R-KONF-05: po 5 min U-01 → K2; U-03 po śnie → U-01
  SPRAWDZ(ekranPoStarcie(PowodWybudzenia::ZEGAR, Ekran::USTAWIENIA, true, 301, 600) == Ekran::K2);
  SPRAWDZ(ekranPoStarcie(PowodWybudzenia::ZEGAR, Ekran::USTAWIENIA, true, 100, 600) == Ekran::USTAWIENIA);
  SPRAWDZ(ekranPoStarcie(PowodWybudzenia::ZEGAR, Ekran::FORMULARZ, true, 10, 600) == Ekran::USTAWIENIA);
}

// ---------- etapy 2–3: formularz z dyrygentem na atrapach ----------
class GniazdoFormularza : public serwer::GniazdoHttp {
 public:
  bool slucha = false;
  int startow = 0, stopow = 0;
  std::deque<Zadanie> doWyslania;
  std::vector<serwer::Odpowiedz> odp;
  bool start(uint16_t) override { startow++; slucha = true; return true; }
  void stop() override { stopow++; slucha = false; }
  void obsluz(const std::function<serwer::Odpowiedz(const std::string&)>&) override {}
  void obsluzZadania(const std::function<serwer::Odpowiedz(const Zadanie&)>& f) override {
    if (!slucha || doWyslania.empty()) return;
    Zadanie z = doWyslania.front();
    doWyslania.pop_front();
    odp.push_back(f(z));
  }
};
static uint32_t epL(int d, int h, int m) {
  std::tm t{};
  t.tm_year = 126, t.tm_mon = 10, t.tm_mday = d, t.tm_hour = h, t.tm_min = m;
  t.tm_isdst = -1;
  return (uint32_t)mktime(&t);
}
static std::string tokenAtrapy() {
  uint8_t b[16];
  for (int i = 0; i < 16; i++) b[i] = (uint8_t)(0x12345678u >> (8 * (i % 4)));
  return serwer::token(b);
}
struct PanelP07 {
  Panel p;
  sekrety::Wynik s;
  PanelP07() {
    s = sekrety::wczytaj(p.nvs, daneKompilacji());
    p.siec.ssid = "Dom";
    p.siec.haslo = HASLO_WIFI;
  }
  KonfiguracjaPanelu konf() {
    s = sekrety::wczytaj(p.nvs, daneKompilacji());
    KonfiguracjaPanelu k = konfZ(s);
    k.ssid = s.d.ssid;
    k.hasloWifi = s.d.hasloWifi;
    return k;
  }
  void zegarem(int h, int m) {
    p.zegar.epoch = epL(5, h, m);
    p.zegar.msTeraz = 0;
    p.start.przyczynaStartu = PrzyczynaStartu::USPIENIE;
    p.start.powodWybudzenia = PowodWybudzenia::ZEGAR;
    p.siec.dodaj("/api/status", 200, status());
    p.siec.dodaj(sciezkaListy(), 200, "{\"source\":\"edf\",\"days\":[]}");
    zerujPrzedSnem(p.start.l, identyfikatorWersji(p.start.napisWersji));
    Dyrygent(p.adaptery(), konf()).wybudzenie();
  }
  void nacisnij(Przycisk pr, bool dlugo, int h, int m) {
    uint8_t bit = (uint8_t)(1u << (int)pr);
    p.port.log.clear();
    p.zegar.epoch = epL(5, h, m);
    p.zegar.msTeraz = 0;
    p.start.powodWybudzenia = PowodWybudzenia::PRZYCISK;
    p.start.budzace = bit;
    p.przyciski.skrypt = {{0, bit}, {dlugo ? 2300u : 300u, 0}};
    zerujPrzedSnem(p.start.l, identyfikatorWersji(p.start.napisWersji));
    Dyrygent(p.adaptery(), konf()).wybudzenie();
  }
  bool log(const std::string& t) const {
    for (auto& l : p.port.log)
      if (l.find(t) != std::string::npos) return true;
    return false;
  }
  Ekran ekran() const { return (Ekran)p.start.stan.ekran; }
};
static serwer::GniazdoHttp::Zadanie zad(const char* m, const std::string& s, const std::string& c = "") {
  return serwer::GniazdoHttp::Zadanie{m, s, c};
}

static void formularzDyrygent() {
  GniazdoFormularza g;
  serwer::GniazdoHttp* przed = serwer::gniazdo();
  serwer::ustawGniazdo(&g);
  const std::string sc = "/u/" + tokenAtrapy() + "/";
  {  // „Sprzęt” (ostatni wiersz) ▶ → U-01 → ▶ U-03: strona, POST pól pliku, 15 min → U-01
    PanelP07 t;
    t.p.pliki.pliki["/konfiguracja.txt"] = "co2=nie\nwymiana.rura=2026-01-01\n";
    t.zegarem(9, 0);
    t.p.start.stan.ekran = (uint8_t)Ekran::SPRZET;
    t.p.nvs.liczbyU["cpap/sp_kur"] = 1;
    t.nacisnij(Przycisk::PRAWY, false, 9, 1);
    SPRAWDZ(t.ekran() == Ekran::USTAWIENIA);
    g.doWyslania = {zad("GET", "/"), zad("GET", sc),
                    zad("POST", sc, "profil=publiczny&wymiana.filtr=2026-10-01&wymiana.rura=&okres.maska=0&ssid=Dom"
                                    "&wifi_haslo=&ab_adres=192.168.1.20&ab_konto=admin&ab_haslo=")};
    t.nacisnij(Przycisk::PRAWY, true, 9, 3);  // ▶ 2 s (Wi-Fi)
    SPRAWDZ(t.log("formularz: serwer do 09:18"));
    SPRAWDZ(g.odp.size() == 3);
    if (g.odp.size() == 3) {
      SPRAWDZ(g.odp[0].kod == 404 && g.odp[1].kod == 200 && g.odp[2].kod == 200);
      SPRAWDZ(zawiera(g.odp[1].cialo, "value=\"Dom\"") && zawiera(g.odp[1].cialo, "placeholder=\"ustawione\""));
      SPRAWDZ(zawiera(g.odp[2].cialo, "Popraw pola") && zawiera(g.odp[2].cialo, "Liczba miesięcy od 1 do 60."));
      for (auto& o : g.odp) SPRAWDZ(!maHaslo(o.cialo));  // R-KONF-38
    }
    SPRAWDZ(t.p.pliki.tresc("/konfiguracja.txt") ==
            "co2=nie\nwymiana.rura=2026-01-01\nprofil=publiczny\nwymiana.filtr=2026-10-01\n");  // wiersz rury zostaje
    SPRAWDZ(t.p.nvs.teksty["sekrety/zrodlo"] == "k");  // sieć i AirBridge bez zmian: bez zapisu sekretów
    SPRAWDZ(t.log("formularz: minęło 15 min") && t.ekran() == Ekran::USTAWIENIA && !g.slucha);
    for (auto& l : t.p.port.log) SPRAWDZ(!maHaslo(l));
    g.odp.clear();
  }
  {  // nowe hasło AirBridge: sprawdzenie GET /api/status przed zapisem; 401 → poprzednie zostaje (R-KONF-37)
    PanelP07 t;
    t.zegarem(9, 0);
    t.p.start.stan.ekran = (uint8_t)Ekran::USTAWIENIA;
    t.p.siec.odpowiedzi.clear();
    t.p.siec.dodaj("/api/status", 401, "");
    g.doWyslania = {zad("POST", sc, "ab_adres=192.168.1.21&ab_haslo=Nowe-Ab-1")};
    t.nacisnij(Przycisk::PRAWY, true, 9, 3);  // ▶ 2 s (Wi-Fi)
    SPRAWDZ(g.odp.size() == 1 && zawiera(g.odp[0].cialo, "AirBridge pod 192.168.1.21 odrzucił logowanie."));
    SPRAWDZ(t.p.nvs.teksty["sekrety/ab"] == "192.168.1.20" && t.p.nvs.teksty["sekrety/ab_h"] == HASLO_AB);
    bool doNowego = false;
    for (auto& u : t.p.siec.zadania) if (zawiera(u, "http://192.168.1.21/api/status")) doNowego = true;
    SPRAWDZ(doNowego);
    if (!g.odp.empty()) SPRAWDZ(!zawiera(g.odp[0].cialo, "Nowe-Ab-1"));
    for (auto& l : t.p.port.log) SPRAWDZ(!zawiera(l, "Nowe-Ab-1"));
    g.odp.clear();
    // to samo z odpowiedzią 200: zapis w NVS ze źródłem „f”
    t.p.siec.odpowiedzi.clear();
    t.p.siec.dodaj("/api/status", 200, status());
    g.doWyslania = {zad("POST", sc, "ab_adres=192.168.1.21&ab_haslo=Nowe-Ab-1")};
    t.p.start.stan.ekran = (uint8_t)Ekran::USTAWIENIA;
    t.nacisnij(Przycisk::PRAWY, true, 9, 30);  // ▶ 2 s (Wi-Fi)
    SPRAWDZ(g.odp.size() == 1 && zawiera(g.odp[0].cialo, "AirBridge odpowiada pod 192.168.1.21. Zapisano."));
    SPRAWDZ(t.p.nvs.teksty["sekrety/ab"] == "192.168.1.21" && t.p.nvs.teksty["sekrety/ab_h"] == "Nowe-Ab-1" &&
            t.p.nvs.teksty["sekrety/zrodlo"] == "f");
    SPRAWDZ(sekrety::wczytaj(t.p.nvs, daneKompilacji()).d.abAdres == "192.168.1.21");  // secrets.h nie nadpisuje
    g.odp.clear();
  }
  {  // nowa sieć Wi-Fi, której panel nie widzi: powrót do poprzedniej, formularz od nowa (R-KONF-10)
    PanelP07 t;
    t.p.siec.widoczne = {{"Dom", HASLO_WIFI}};
    t.zegarem(9, 0);
    t.p.start.stan.ekran = (uint8_t)Ekran::USTAWIENIA;
    g.doWyslania = {zad("POST", sc, "ssid=Inna&wifi_haslo=Haslo-Innej-1")};
    t.nacisnij(Przycisk::PRAWY, true, 9, 3);  // ▶ 2 s (Wi-Fi)
    SPRAWDZ(g.odp.size() == 1 && zawiera(g.odp[0].cialo, "Panel sprawdza sieć Inna."));
    SPRAWDZ(t.log("formularz: nowa sieć Inna odrzucona, poprzednia zostaje"));
    SPRAWDZ(t.p.siec.ssid == "Dom" && t.p.siec.haslo == HASLO_WIFI);
    SPRAWDZ(t.p.nvs.teksty["sekrety/ssid"] == "Dom" && g.startow >= 2);  // formularz otwarty ponownie
    for (auto& l : t.p.port.log) SPRAWDZ(!zawiera(l, "Haslo-Innej-1"));
    g.odp.clear();
    // ta sama sieć widoczna: zapis
    t.p.siec.widoczne["Inna"] = "Haslo-Innej-1";
    t.p.siec.odpowiedzi.clear();
    t.p.siec.dodaj("/api/status", 200, status());
    t.p.siec.dodaj(sciezkaListy(), 200, "{\"source\":\"edf\",\"days\":[]}");
    t.p.start.stan.ekran = (uint8_t)Ekran::USTAWIENIA;
    g.doWyslania = {zad("POST", sc, "ssid=Inna&wifi_haslo=Haslo-Innej-1")};
    t.nacisnij(Przycisk::PRAWY, true, 10, 0);  // ▶ 2 s (Wi-Fi)
    SPRAWDZ(t.log("formularz: nowa sieć Inna zapisana"));
    SPRAWDZ(t.p.nvs.teksty["sekrety/ssid"] == "Inna" && t.p.nvs.teksty["sekrety/wifi"] == "Haslo-Innej-1");
    SPRAWDZ(t.p.siec.ssid == "Inna");
    g.odp.clear();
  }
  serwer::ustawGniazdo(przed);
}

// ---------- etap 4: sieć panelu (U-02), tylko ręcznie ----------
static void siecPaneluMapa() {
  KontekstNaw k;
  for (Ekran e : {Ekran::USTAWIENIA, Ekran::TEST}) {
    Przejscie pr = nacisniecie(e, Przycisk::LEWY, true, k);
    SPRAWDZ(pr.ekran == Ekran::SIEC_PANELU && pr.akcja == Akcja::SIEC_PANELU_START);
  }
  SPRAWDZ(nacisniecie(Ekran::TEST, Przycisk::LEWY, false, k).ekran == Ekran::USTAWIENIA);  // ◀ krótko bez zmian
  // ekran dnia: ◀ 2 s tylko bez konfiguracji (pierwsze uruchomienie), inaczej jak dotąd
  SPRAWDZ(nacisniecie(Ekran::DZIEN, Przycisk::LEWY, true, k).akcja == Akcja::NIC);
  k.bezKonfiguracji = true;
  SPRAWDZ(nacisniecie(Ekran::DZIEN, Przycisk::LEWY, true, k).akcja == Akcja::SIEC_PANELU_START);
  k.nocny = true;  // także na ekranie nocnym
  SPRAWDZ(nacisniecie(Ekran::DZIEN, Przycisk::LEWY, true, k).akcja == Akcja::SIEC_PANELU_START);
  k = KontekstNaw();
  // U-02: ◀ (krótko i 2 s) anuluj, ↻ 2 s od nowa, ↻ ekran dnia (też anuluje), ▶ nic
  for (bool d : {false, true}) SPRAWDZ(nacisniecie(Ekran::SIEC_PANELU, Przycisk::LEWY, d, k).akcja == Akcja::SIEC_PANELU_ANULUJ);
  SPRAWDZ(nacisniecie(Ekran::SIEC_PANELU, Przycisk::SRODEK, true, k).akcja == Akcja::SIEC_PANELU_START);
  SPRAWDZ(nacisniecie(Ekran::SIEC_PANELU, Przycisk::SRODEK, false, k).akcja == Akcja::SIEC_PANELU_ANULUJ &&
          nacisniecie(Ekran::SIEC_PANELU, Przycisk::SRODEK, false, k).ekran == Ekran::DZIEN);
  for (bool d : {false, true}) SPRAWDZ(nacisniecie(Ekran::SIEC_PANELU, Przycisk::PRAWY, d, k).akcja == Akcja::NIC);
  // po śnie nigdy U-02 (sieć panelu nie przeżywa snu, nie startuje sama)
  SPRAWDZ(ekranPoStarcie(PowodWybudzenia::ZEGAR, Ekran::SIEC_PANELU, true, 10, 600) == Ekran::USTAWIENIA);
  SPRAWDZ(ekranPoStarcie(PowodWybudzenia::START, Ekran::SIEC_PANELU, true, 10, 600) == Ekran::DZIEN);
  // kod QR dołączenia do sieci (format WIFI:, ucieczka znaków specjalnych)
  SPRAWDZ(kodWifi("cpap-eink-A1B2", "abcdefghjkmn") == "WIFI:T:WPA;S:cpap-eink-A1B2;P:abcdefghjkmn;;");
  SPRAWDZ(kodWifi("a;b", "c:d\"e,\\") == "WIFI:T:WPA;S:a\\;b;P:c\\:d\\\"e\\,\\\\;;");
  // strony serwera w trybie jasnym (telefon w trybie ciemnym: przycisk był biały na białym)
  std::string st = formularz::strona(formularz::Pola(), "/u/t/", "", {});
  SPRAWDZ(zawiera(st, "<meta name=\"color-scheme\" content=\"light\">") && zawiera(st, ":root{color-scheme:light}"));
  SPRAWDZ(zawiera(st, "appearance:none;background:#000;color:#fff") && zawiera(st, "color:#000;background:#fff}"));
  serwer::Sesja rs;
  rs.otworz("tok", 0, "192.168.1.50", serwer::Tryb::PDF, "/raporty/2026-11-04-L-01.pdf",
            {{"/raporty/2026-11-04-L-01.pdf", "2026-11-04"}}, "2026-11-05", "Temat", "", false);
  std::string rr = rs.obsluz("/r/tok/", 1).cialo;
  SPRAWDZ(zawiera(rr, "color-scheme") && zawiera(rr, "background:#000;color:#fff"));
  // nowa nazwa sieci z pustym hasłem: komunikat przy polu, bez zapisu ze starym hasłem; ta sama nazwa: bez zmiany
  sekrety::Dane ob = daneKompilacji();
  auto w = formularz::waliduj(formularz::parsujCialo("ssid=Inna&wifi_haslo="), ob, "2026-11-05");
  SPRAWDZ(!w.ok() && w.bledy["wifi_haslo"] == "Nowa sieć: wpisz jej hasło albo zaznacz „bez hasła”." &&
          w.sekrety.ssid == "Dom" && w.sekrety.hasloWifi == HASLO_WIFI && !w.zmianaSieci);
  w = formularz::waliduj(formularz::parsujCialo("ssid=Dom&wifi_haslo="), ob, "2026-11-05");
  SPRAWDZ(w.ok() && w.sekrety.hasloWifi == HASLO_WIFI && !w.zmianaSieci);
  w = formularz::waliduj(formularz::parsujCialo("ssid=Inna&wifi_haslo=&wifi_bez=1"), ob, "2026-11-05");
  SPRAWDZ(w.ok() && w.sekrety.ssid == "Inna" && w.sekrety.hasloWifi.empty());
  formularz::Pola pz = formularz::zPliku("", ob);
  SPRAWDZ(zawiera(formularz::strona(pz, "/u/t/", "", {}), "Puste pole = hasło bez zmian."));
  SPRAWDZ(!zawiera(formularz::strona(formularz::zPliku("", sekrety::Dane()), "/u/t/", "", {}), "Puste pole = hasło"));
}

static int ileLog(const PanelP07& t, const std::string& x) {
  int n = 0;
  for (auto& l : t.p.port.log) n += l.find(x) != std::string::npos;
  return n;
}
static std::string haslaAtrapy() {  // 12 znaków z losowa() = 0x12345678 (atrapa bez kolejki `losowe`)
  static const char A[] = "23456789abcdefghjkmnpqrstuvwxyz";
  return std::string(12, A[0x12345678u % 31]);
}
// Naciśnięcie budzące ◀ 2 s, potem dalszy skrypt przycisków w tym samym wybudzeniu (ms od startu).
static void skrypt(PanelP07& t, std::vector<std::pair<uint32_t, uint8_t>> dalej, int h, int m) {
  uint8_t bit = (uint8_t)(1u << (int)Przycisk::LEWY);
  t.p.port.log.clear();
  t.p.zegar.epoch = epL(5, h, m);
  t.p.zegar.msTeraz = 0;
  t.p.start.powodWybudzenia = PowodWybudzenia::PRZYCISK;
  t.p.start.budzace = bit;
  t.p.przyciski.skrypt = {{0, bit}, {2300u, 0}};
  for (auto& d : dalej) t.p.przyciski.skrypt.push_back(d);
  zerujPrzedSnem(t.p.start.l, identyfikatorWersji(t.p.start.napisWersji));
  Dyrygent(t.p.adaptery(), t.konf()).wybudzenie();
}

static void siecPaneluDyrygent() {
  GniazdoFormularza g;
  serwer::GniazdoHttp* przed = serwer::gniazdo();
  serwer::ustawGniazdo(&g);
  const std::string sc = "/u/" + tokenAtrapy() + "/";
  const uint8_t L = (uint8_t)(1u << (int)Przycisk::LEWY);
  {  // U-01 → ◀ 2 s: AP, dwa kody, hasło tylko na U-02; GET strony; 15 min → U-01, sieć domowa bez zmian
    PanelP07 t;
    t.zegarem(9, 0);
    t.p.start.stan.ekran = (uint8_t)Ekran::USTAWIENIA;
    g.doWyslania = {zad("GET", "/favicon.ico"), zad("GET", sc)};
    int odsw = t.p.ekran.odswiezen;
    skrypt(t, {}, 9, 1);
    SPRAWDZ(t.p.siec.apStartow == 1 && t.p.siec.apSsid == "cpap-eink-A1B2");
    SPRAWDZ(t.p.siec.apHaslo.size() >= 12 && t.p.siec.apHaslo == haslaAtrapy());
    SPRAWDZ(t.log("sieć panelu: cpap-eink-A1B2 pod 192.168.4.1, formularz do 09:16"));
    SPRAWDZ(g.odp.size() == 2 && g.odp[1].kod == 200 && zawiera(g.odp[1].cialo, "value=\"Dom\""));
    for (auto& o : g.odp) SPRAWDZ(!maHaslo(o.cialo) && !zawiera(o.cialo, t.p.siec.apHaslo));
    SPRAWDZ(t.log("sieć panelu: minęło 15 min") && t.ekran() == Ekran::USTAWIENIA && !t.p.siec.apTeraz && !g.slucha);
    SPRAWDZ(t.p.siec.ssid == "Dom" && t.p.siec.haslo == HASLO_WIFI);  // powrót do poprzedniej sieci
    SPRAWDZ(t.p.nvs.teksty["sekrety/ssid"] == "Dom" && t.p.nvs.teksty["sekrety/zrodlo"] == "k");
    SPRAWDZ(t.p.ekran.odswiezen >= odsw + 2);  // U-02 narysowany, potem U-01
    for (auto& l : t.p.port.log) SPRAWDZ(!zawiera(l, t.p.siec.apHaslo) && !maHaslo(l));  // hasła nigdy w logu
    SPRAWDZ(!ileLog(t, "siec.polacz"));
    g.odp.clear();
  }
  {  // U-05 → ◀ 2 s, po minucie ◀ anuluj: powrót do U-05, AP wyłączony od razu
    PanelP07 t;
    t.zegarem(9, 0);
    t.p.start.stan.ekran = (uint8_t)Ekran::TEST;
    skrypt(t, {{60000u, L}, {60300u, 0}}, 9, 1);
    SPRAWDZ(t.p.siec.apStartow == 1 && t.log("sieć panelu: anulowana ◀") && t.ekran() == Ekran::TEST);
    SPRAWDZ(!t.p.siec.apTeraz && !t.log("minęło 15 min") && t.p.zegar.msTeraz < 120000u);
    SPRAWDZ(t.p.siec.ssid == "Dom");
  }
  {  // hasło i token losowe: dwa uruchomienia → różne hasła (kolejka losowa() atrapy)
    PanelP07 t;
    t.zegarem(9, 0);
    t.p.siec.losowe = {11, 22, 33, 44, 55, 66, 77, 88, 99, 111, 222, 333};
    t.p.start.stan.ekran = (uint8_t)Ekran::USTAWIENIA;
    skrypt(t, {{30000u, (uint8_t)(1u << (int)Przycisk::SRODEK)}, {32500u, 0}}, 9, 1);  // ↻ 2 s na U-02: od nowa
    SPRAWDZ(t.p.siec.apStartow == 2);
    std::string h2 = t.p.siec.apHaslo;
    PanelP07 u;
    u.zegarem(9, 0);
    u.p.siec.losowe = {11, 22, 33, 44, 55, 66, 77, 88, 99, 111, 222, 333};
    u.p.start.stan.ekran = (uint8_t)Ekran::USTAWIENIA;
    skrypt(u, {}, 9, 1);
    SPRAWDZ(h2.size() == 12 && u.p.siec.apHaslo.size() == 12 && h2 != u.p.siec.apHaslo);
    std::string zn = "23456789abcdefghjkmnpqrstuvwxyz";
    for (char c : h2) SPRAWDZ(zn.find(c) != std::string::npos);
  }
  {  // sieć zapisana, ale nieosiągalna: start zasilania bez AP (tylko ręcznie), E-01 zwykłym trybem
    PanelP07 t;
    t.p.siec.siecJest = false;
    t.p.zegar.epoch = epL(5, 9, 0);
    t.p.zegar.msTeraz = 0;
    t.p.start.przyczynaStartu = PrzyczynaStartu::ZASILANIE;
    t.p.start.powodWybudzenia = PowodWybudzenia::START;
    zerujPrzedSnem(t.p.start.l, identyfikatorWersji(t.p.start.napisWersji));
    Dyrygent(t.p.adaptery(), t.konf()).wybudzenie();
    SPRAWDZ(t.p.siec.apStartow == 0 && !t.log("pierwszy start bez sieci") && t.ekran() == Ekran::DZIEN);
  }
  {  // restart w trakcie (zanik zasilania): zwykły start, AP nie rusza, dane bez zmian
    PanelP07 t;
    t.zegarem(9, 0);
    t.p.start.stan.ekran = (uint8_t)Ekran::SIEC_PANELU;
    t.p.zegar.epoch = epL(5, 9, 5);
    t.p.zegar.msTeraz = 0;
    t.p.start.przyczynaStartu = PrzyczynaStartu::ZASILANIE;
    t.p.start.powodWybudzenia = PowodWybudzenia::START;
    zerujPrzedSnem(t.p.start.l, identyfikatorWersji(t.p.start.napisWersji));
    Dyrygent(t.p.adaptery(), t.konf()).wybudzenie();
    SPRAWDZ(t.p.siec.apStartow == 0 && t.ekran() != Ekran::SIEC_PANELU && t.p.nvs.teksty["sekrety/ssid"] == "Dom");
  }
  {  // zła sieć: 60 s, powrót do U-02 z nowym hasłem, stare dane w NVS; potem zły AirBridge; potem sukces
    PanelP07 t;
    t.p.siec.widoczne = {{"Dom", HASLO_WIFI}};
    t.zegarem(9, 0);
    t.p.start.stan.ekran = (uint8_t)Ekran::USTAWIENIA;
    g.doWyslania = {zad("POST", sc, "ssid=Inna&wifi_haslo=Haslo-Innej-1")};
    skrypt(t, {}, 9, 1);
    SPRAWDZ(g.odp.size() == 1 && zawiera(g.odp[0].cialo, "Panel wyłącza teraz swoją sieć i łączy się z siecią Inna."));
    SPRAWDZ(t.log("sieć panelu: nowa sieć Inna odrzucona, poprzednia zostaje") && t.p.siec.apStartow == 2);
    SPRAWDZ(t.p.nvs.teksty["sekrety/ssid"] == "Dom" && t.p.siec.ssid == "Dom" && t.p.siec.haslo == HASLO_WIFI);
    SPRAWDZ(t.ekran() == Ekran::USTAWIENIA);  // po 15 min nowej sesji powrót
    for (auto& l : t.p.port.log) SPRAWDZ(!zawiera(l, "Haslo-Innej-1"));
    g.odp.clear();
    // zły AirBridge (401) w nowej sieci: stare dane zostają
    t.p.siec.widoczne["Inna"] = "Haslo-Innej-1";
    t.p.siec.odpowiedzi.clear();
    t.p.siec.dodaj("/api/status", 401, "");
    g.doWyslania = {zad("POST", sc, "ssid=Inna&wifi_haslo=Haslo-Innej-1")};
    t.p.start.stan.ekran = (uint8_t)Ekran::USTAWIENIA;
    skrypt(t, {}, 10, 0);
    SPRAWDZ(t.log("sieć panelu: nowa sieć Inna odrzucona") && t.p.nvs.teksty["sekrety/ssid"] == "Dom");
    SPRAWDZ(t.p.siec.ssid == "Dom" && t.p.nvs.teksty["sekrety/zrodlo"] == "k");
    g.odp.clear();
    // sukces: zapis „f”, ekran dnia, pobranie z nowej sieci
    t.p.siec.odpowiedzi.clear();
    t.p.siec.dodaj("/api/status", 200, status());
    t.p.siec.dodaj(sciezkaListy(), 200, "{\"source\":\"edf\",\"days\":[]}");
    g.doWyslania = {zad("POST", sc, "ssid=Inna&wifi_haslo=Haslo-Innej-1")};
    t.p.start.stan.ekran = (uint8_t)Ekran::USTAWIENIA;
    skrypt(t, {}, 11, 0);
    SPRAWDZ(t.log("sieć panelu: nowa sieć Inna zapisana, zwykła praca") && t.ekran() == Ekran::DZIEN);
    SPRAWDZ(t.p.nvs.teksty["sekrety/ssid"] == "Inna" && t.p.nvs.teksty["sekrety/wifi"] == "Haslo-Innej-1" &&
            t.p.nvs.teksty["sekrety/zrodlo"] == "f" && t.p.siec.ssid == "Inna");
    SPRAWDZ(!t.p.siec.apTeraz);
    for (auto& l : t.p.port.log) SPRAWDZ(!zawiera(l, "Haslo-Innej-1") && !zawiera(l, t.p.siec.apHaslo));
    g.odp.clear();
  }
  {  // noc (slot nocny): uruchomienie bez buzzera, powrót po 15 min także w nocy
    PanelP07 t;
    t.zegarem(9, 0);
    t.p.start.stan.ekran = (uint8_t)Ekran::USTAWIENIA;
    int pisk = t.p.dzwiek.piskow;
    skrypt(t, {}, 23, 45);
    SPRAWDZ(t.p.siec.apStartow == 1 && t.p.dzwiek.piskow == pisk && t.log("(noc, bez buzzera)"));
    SPRAWDZ(t.log("sieć panelu: minęło 15 min") && t.ekran() == Ekran::USTAWIENIA);
    // w dzień z buzzerem
    t.p.start.stan.ekran = (uint8_t)Ekran::USTAWIENIA;
    skrypt(t, {}, 10, 0);
    SPRAWDZ(t.p.dzwiek.piskow > pisk);
  }
  {  // 5 złych żądań: koniec sieci panelu
    PanelP07 t;
    t.zegarem(9, 0);
    t.p.start.stan.ekran = (uint8_t)Ekran::USTAWIENIA;
    g.doWyslania = {zad("GET", "/"), zad("GET", "/a"), zad("GET", "/u/x/"), zad("POST", "/u/y/"), zad("GET", "/b")};
    skrypt(t, {}, 9, 1);
    SPRAWDZ(t.log("sieć panelu: 5 złych żądań") && !t.p.siec.apTeraz && t.p.zegar.msTeraz < 120000u);
    g.odp.clear();
  }
  {  // komenda `O` z portu w czasie U-02 najpierw zamyka sieć panelu i serwer formularza
    PanelP07 t;
    t.zegarem(9, 0);
    t.p.start.stan.ekran = (uint8_t)Ekran::USTAWIENIA;
    t.p.port.znaki = {'O'};
    skrypt(t, {}, 9, 1);
    SPRAWDZ(t.p.siec.apStartow == 1 && t.log("sieć panelu: wyłączona") && !t.log("minęło 15 min"));
    SPRAWDZ(t.ekran() == Ekran::TEST && !g.slucha && !t.p.siec.apTeraz);
  }
  {  // radio nie startuje AP: komunikat, bez czuwania 15 min
    PanelP07 t;
    t.zegarem(9, 0);
    t.p.siec.apMozliwy = false;
    t.p.start.stan.ekran = (uint8_t)Ekran::USTAWIENIA;
    skrypt(t, {}, 9, 1);
    SPRAWDZ(t.log("sieć panelu: nie mogę włączyć") && t.p.zegar.msTeraz < 120000u);
  }
  {  // pierwsze uruchomienie bez danych: instrukcja zamiast błędów, bez pobierania i E-01; AP tylko ręcznie
    PanelP07 t;
    t.p.nvs.teksty.clear();
    t.p.siec.ssid.clear();
    t.p.siec.haslo.clear();
    t.p.siec.widoczne = {{"Dom", HASLO_WIFI}};
    auto konfPusta = [&t]() {
      sekrety::Wynik s = sekrety::wczytaj(t.p.nvs, sekrety::Dane());
      KonfiguracjaPanelu k = konfZ(s);
      k.ssid = s.d.ssid;
      k.hasloWifi = s.d.hasloWifi;
      return k;
    };
    t.p.zegar.epoch = epL(5, 9, 0);
    t.p.zegar.msTeraz = 0;
    t.p.start.przyczynaStartu = PrzyczynaStartu::ZASILANIE;
    t.p.start.powodWybudzenia = PowodWybudzenia::START;
    zerujPrzedSnem(t.p.start.l, identyfikatorWersji(t.p.start.napisWersji));
    int pisk0 = t.p.dzwiek.piskow;
    Dyrygent(t.p.adaptery(), konfPusta()).wybudzenie();
    // pierwszy start: start zasilania bez żadnej sieci → sieć panelu sama, po 15 min instrukcja na ekranie dnia
    SPRAWDZ(t.log("pierwszy start bez sieci: sieć panelu sama") && t.p.siec.apStartow == 1);
    SPRAWDZ(t.log("sieć panelu: minęło 15 min") && !t.p.siec.apTeraz && t.p.dzwiek.piskow == pisk0);
    SPRAWDZ(t.log("pierwsza konfiguracja: bez pobierania") && !ileLog(t, "AirBridge /api"));
    SPRAWDZ(t.ekran() == Ekran::DZIEN && t.p.start.stan.ab.bladOd == 0);
    // restart programowy (nie start zasilania) bez sieci: bez AP
    t.p.zegar.msTeraz = 0;
    t.p.start.przyczynaStartu = PrzyczynaStartu::PROGRAMOWY;
    zerujPrzedSnem(t.p.start.l, identyfikatorWersji(t.p.start.napisWersji));
    Dyrygent(t.p.adaptery(), konfPusta()).wybudzenie();
    SPRAWDZ(t.p.siec.apStartow == 1 && t.ekran() == Ekran::DZIEN);
    // kolejny start zasilania: znów sama
    t.p.zegar.msTeraz = 0;
    t.p.start.przyczynaStartu = PrzyczynaStartu::ZASILANIE;
    zerujPrzedSnem(t.p.start.l, identyfikatorWersji(t.p.start.napisWersji));
    Dyrygent(t.p.adaptery(), konfPusta()).wybudzenie();
    SPRAWDZ(t.p.siec.apStartow == 2 && t.ekran() == Ekran::DZIEN);
    // zegarem jeszcze raz: dalej bez pobierania (bez E-01)
    t.p.zegar.epoch = epL(5, 12, 0);
    t.p.zegar.msTeraz = 0;
    t.p.start.przyczynaStartu = PrzyczynaStartu::USPIENIE;
    t.p.start.powodWybudzenia = PowodWybudzenia::ZEGAR;
    zerujPrzedSnem(t.p.start.l, identyfikatorWersji(t.p.start.napisWersji));
    Dyrygent(t.p.adaptery(), konfPusta()).wybudzenie();
    SPRAWDZ(t.p.siec.apStartow == 2 && t.p.start.stan.ab.bladOd == 0);  // wybudzenie zegarem: bez AP
    // ◀ 2 s na ekranie dnia: sieć panelu, formularz pierwszej konfiguracji, sukces
    t.p.siec.odpowiedzi.clear();
    t.p.siec.dodaj("/api/status", 200, status());
    t.p.siec.dodaj(sciezkaListy(), 200, "{\"source\":\"edf\",\"days\":[]}");
    g.doWyslania = {zad("POST", sc, "ssid=Dom&wifi_haslo=" + std::string(HASLO_WIFI) +
                                        "&ab_adres=192.168.1.20&ab_konto=admin&ab_haslo=" + HASLO_AB)};
    uint8_t bit = L;
    t.p.port.log.clear();
    t.p.zegar.epoch = epL(5, 12, 10);
    t.p.zegar.msTeraz = 0;
    t.p.start.powodWybudzenia = PowodWybudzenia::PRZYCISK;
    t.p.start.budzace = bit;
    t.p.przyciski.skrypt = {{0, bit}, {2300u, 0}};
    zerujPrzedSnem(t.p.start.l, identyfikatorWersji(t.p.start.napisWersji));
    Dyrygent(t.p.adaptery(), konfPusta()).wybudzenie();
    SPRAWDZ(t.p.siec.apStartow == 3 && t.log("sieć panelu: nowa sieć Dom zapisana"));
    SPRAWDZ(t.p.nvs.teksty["sekrety/zrodlo"] == "f" && t.p.nvs.teksty["sekrety/ab"] == "192.168.1.20");
    SPRAWDZ(sekrety::wczytaj(t.p.nvs, sekrety::Dane()).d.ssid == "Dom");
    for (auto& l : t.p.port.log) SPRAWDZ(!maHaslo(l));
    g.odp.clear();
  }
  serwer::ustawGniazdo(przed);
}

int main() {
  sekretyLogika();
  panelWlasciciela();
  formularzStrona();
  formularzCialo();
  formularzWalidacja();
  formularzSesja();
  mapaUstawien();
  formularzDyrygent();
  siecPaneluMapa();
  siecPaneluDyrygent();
  printf("testy_konfiguracja_z_telefonu: %d ok, %d błędów\n", ok, zle);
  return zle ? 1 : 0;
}
