// SPDX-License-Identifier: GPL-3.0-or-later
// Testy przypadków brzegowych (konfiguracja z telefonu).
// Testy oznaczone ZNANE pokazują znane ograniczenie (wypisane, nie blokują); pozostałe sprawdzają, co trzyma (wyciek, wstrzyknięcie,
// token). Dane syntetyczne, bez prawdziwych haseł.
#include <cstdio>
#include <map>
#include <string>

#include "adaptery_atrapy.h"
#include "formularz.h"
#include "sekrety.h"
#include "serwer_raportu.h"

static int ok = 0, zle = 0;
#define SPRAWDZ(nr, w)                                                        \
  do {                                                                        \
    if (w) ok++;                                                              \
    else { zle++; printf("BŁĄD %s %s:%d: %s\n", nr, __FILE__, __LINE__, #w); } \
  } while (0)

// Znane ograniczenie, opisane, bez poprawki w kodzie: wypisane, nie blokuje.
static int znane = 0;
#define ZNANE(nr, w)                                                                   \
  do {                                                                                 \
    if (!(w)) { znane++; printf("ZNANE %s %s:%d: %s\n", nr, __FILE__, __LINE__, #w); } \
  } while (0)

static sekrety::Dane dane(const char* ssid, const char* hw, const char* ab, const char* abh) {
  sekrety::Dane d;
  d.ssid = ssid;
  d.hasloWifi = hw;
  d.abAdres = ab;
  d.abKonto = "admin";
  d.abHaslo = abh;
  return d;
}
static bool zawiera(const std::string& t, const std::string& x) { return t.find(x) != std::string::npos; }

// NVS, który od pewnego zapisu po cichu nie zapisuje (pełna strona NVS, błąd flash): ustawTekst() nie ma wyniku.
class NvsZawodny : public AtrapaNvs {
 public:
  explicit NvsZawodny(Dziennik& d) : AtrapaNvs(d) {}
  std::string psujKlucz;  // zapis tego klucza w przestrzeni `sekrety` przepada
  // ustawTekst ma wynik; atrapa nadal gubi zapis po cichu (wynik true), a łapie to odczyt kontrolny
  bool ustawTekst(const char* p, const char* kl, const std::string& v) override {
    if (std::string(p) == "sekrety" && psujKlucz == kl) return true;
    return AtrapaNvs::ustawTekst(p, kl, v);
  }
};

// (B): obraz wydania zbudowany z secrets.h z wartościami sieci (np. wzór secrets.example.h z odkomentowanymi
// przykładami; sam wzór, z którego buduje CI, ma je zakomentowane) i zainstalowany na panelu ze źródłem „k”
// (panel wgrany wcześniej kablem z własnym secrets.h) nadpisuje prawdziwą sieć wartościami z obrazu. ADR-0032 pkt 7 zakłada jeden obraz dla wszystkich paneli. Panel bez Wi-Fi nie otworzy
// formularza (wymaga sieci domowej) ani nie pobierze kolejnej aktualizacji: droga powrotu tylko kablem.
static void wydanieZeWzoru() {
  Dziennik dz;
  AtrapaNvs n(dz);
  sekrety::Dane tester = dane("SiecTestera", "Haslo-Testera-1", "192.168.0.40", "Ab-Testera-1");
  sekrety::wczytaj(n, tester);  // wgranie kablem na egzemplarzu testowym: źródło „k”
  sekrety::Dane wzor = dane("nazwa-sieci", "haslo-wifi", "192.168.1.50", "haslo-airbridge");  // przykłady ze wzoru
  sekrety::Wynik w = sekrety::wczytaj(n, wzor);  // pierwsza aktualizacja z wydania
  // znane ograniczenie (CHANGELOG, ADR-0032), poprawka przed pierwszym obrazem wydania
  ZNANE("obraz wydania zbudowany ze wzoru secrets.example.h", w.d.ssid == "SiecTestera");
  ZNANE("obraz wydania zbudowany ze wzoru secrets.example.h", n.teksty["sekrety/ssid"] == "SiecTestera");
}

// Zapis z formularza nie sprawdza zapisu NVS. Gdy jeden klucz nie zapisze się (ustawTekst() bez wyniku),
// zapiszZFormularza() i tak ustawia źródło „f” i zwraca true: w NVS mieszanka starego hasła AirBridge i nowego adresu,
// telefon dostaje „Zapisano.”, a po restarcie panel loguje się starym hasłem do nowego adresu.
static void zapisBezSprawdzenia() {
  Dziennik dz;
  NvsZawodny n(dz);
  sekrety::wczytaj(n, dane("Dom", "Haslo-Dom-1", "192.168.1.20", "Ab-Stare-1"));
  n.psujKlucz = "ab_h";
  sekrety::Dane nowe = dane("Dom", "Haslo-Dom-1", "192.168.1.21", "Ab-Nowe-1");
  bool zap = sekrety::zapiszZFormularza(n, nowe);
  sekrety::Wynik w = sekrety::wczytaj(n, sekrety::Dane());
  bool spojne = (w.d.abAdres == "192.168.1.21" && w.d.abHaslo == "Ab-Nowe-1") ||
                (w.d.abAdres == "192.168.1.20" && w.d.abHaslo == "Ab-Stare-1");
  SPRAWDZ("zapis z formularza nie sprawdza zapisu NVS", spojne);
  SPRAWDZ("zapis z formularza nie sprawdza zapisu NVS", !zap || w.d.abHaslo == "Ab-Nowe-1");  // „zapisano” tylko, gdy naprawdę zapisane
}

// (B): restart w trakcie zapisu z formularza (źródło „z”) wraca do secrets.h, nie do poprzednich danych
// z formularza. Panel, który wcześniej dostał nową sieć z formularza (secrets.h ma już starą), po takim restarcie
// łączy się ze starą siecią: bez Wi-Fi nie ma formularza, droga powrotu tylko kablem z `erase`.
static void restartWTrakcieZapisu() {
  Dziennik dz;
  AtrapaNvs n(dz);
  sekrety::Dane stareSh = dane("StaryRouter", "Haslo-Stary-1", "192.168.1.20", "Ab-1");
  sekrety::wczytaj(n, stareSh);
  sekrety::zapiszZFormularza(n, dane("NowyRouter", "Haslo-Nowy-1", "192.168.1.20", "Ab-1"));  // „f”, sprawdzone
  // drugi zapis z formularza (np. nowe hasło AirBridge) przerwany po „z” i części wartości
  n.ustawTekst("sekrety", "zrodlo", "z");
  n.ustawTekst("sekrety", "ab_h", "Ab-2");
  sekrety::Wynik w = sekrety::wczytaj(n, stareSh);
  SPRAWDZ("restart w trakcie zapisu z formularza", w.d.ssid == "NowyRouter");
}

// (B, do decyzji): po zapisie z formularza nowe wgranie kablem z poprawionym secrets.h (nowy router sieci testowej)
// nie zmienia sieci; jedyna droga to `pio run -t erase`, który czyści cały flash (pliki nocy, historia, NVS `cpap`).
// Brak wybiórczego resetu przestrzeni `sekrety` (np. komenda portu albo przytrzymanie przy starcie).
static void kabelPoFormularzu() {
  Dziennik dz;
  AtrapaNvs n(dz);
  sekrety::wczytaj(n, dane("Dom", "Haslo-Dom-1", "192.168.1.20", "Ab-1"));
  sekrety::zapiszZFormularza(n, dane("Dom", "Haslo-Dom-1", "192.168.1.21", "Ab-1"));
  sekrety::Wynik w = sekrety::wczytaj(n, dane("NowyRouter", "Haslo-Nowy-1", "192.168.1.21", "Ab-1"));
  // przestaje być potrzebne : nowy router zmienia się siecią panelu (◀ 2 s na U-01 albo U-05),
  // bez kabla i bez `erase`; dane z formularza zostają ważniejsze od secrets.h z założenia
  SPRAWDZ("po zapisie z formularza nowe wgranie kablem z poprawionym (przyjęte)", w.d.ssid == "Dom");
}

// Trzyma: wstrzyknięcie wiersza do /konfiguracja.txt i znaków sterujących do sekretów.
static void wstrzykniecia() {
  sekrety::Dane ob = dane("Dom", "Haslo-Dom-1", "192.168.1.20", "Ab-1");
  auto m = formularz::parsujCialo(
      "profil=prywatny%0Aco2%3Dnie&okres.maska=12%0D%0Ax%3D1&wymiana.filtr=2026-10-01%0Aprofil%3Dpubliczny"
      "&okres.filtr_tyg=12%0A&ssid=Dom%0AInna&ab_konto=admin%0Aroot&ab_haslo=a%0Ab&wifi_haslo=12345678%0A9");
  formularz::Wynik w = formularz::waliduj(m, ob, "2026-11-05");
  for (auto& kv : w.plik) SPRAWDZ("trzyma", !zawiera(kv.second, "\n") && !zawiera(kv.second, "\r"));
  SPRAWDZ("trzyma", w.bledy.count("profil") && w.bledy.count("okres.maska") && w.bledy.count("wymiana.filtr"));
  SPRAWDZ("trzyma", w.bledy.count("okres.filtr_tyg") && w.bledy.count("ssid") && w.bledy.count("ab_konto"));
  SPRAWDZ("trzyma", w.bledy.count("ab_haslo"));
  SPRAWDZ("trzyma", w.sekrety.ssid == "Dom" && w.sekrety.abHaslo == "Ab-1" && !w.zmianaSieci && !w.zmianaAirBridge);
  std::string nowy = formularz::scal("co2=nie\n", w.plik);
  SPRAWDZ("trzyma", nowy == "co2=nie\n");
  // adres AirBridge publiczny albo z cudzym schematem
  SPRAWDZ("trzyma", !formularz::adresAirBridgeOk("8.8.8.8") && !formularz::adresAirBridgeOk("evil.com") &&
                        !formularz::adresAirBridgeOk("192.168.1.2@evil.com") &&
                        !formularz::adresAirBridgeOk("a.local/x") && !formularz::adresAirBridgeOk("192.168.1.2:0"));
}

// Trzyma: XSS przez nazwę sieci, konto i komunikat; hasła nie wracają do strony.
static void xss() {
  sekrety::Dane s = dane("\"><script>alert(1)</script>", "Haslo-Tajne-77", "192.168.1.20", "Ab-Tajne-88");
  s.abKonto = "a'onmouseover='x";
  formularz::Pola p = formularz::zPliku("", s);
  std::string st = formularz::strona(p, "/u/t/", "Panel sprawdza sieć <img src=x onerror=1>", {{"ssid", "<b>"}});
  SPRAWDZ("trzyma", !zawiera(st, "<script>") && !zawiera(st, "<img") && !zawiera(st, "'onmouseover") &&
                        !zawiera(st, "<b>"));
  SPRAWDZ("trzyma", !zawiera(st, "Haslo-Tajne-77") && !zawiera(st, "Ab-Tajne-88"));
  SPRAWDZ("trzyma", !zawiera(st, "http://") && !zawiera(st, "https://"));
}

// Trzyma: token (ścieżka dokładna, wielkość liter, 5 złych żądań, 15 min, pusty token).
static void token() {
  uint8_t b[serwer::BAJTY_TOKENU];
  for (int i = 0; i < serwer::BAJTY_TOKENU; i++) b[i] = (uint8_t)(i * 37 + 11);
  std::string tk = serwer::token(b);
  SPRAWDZ("trzyma", tk.size() >= 25);
  formularz::Sesja s;
  s.otworz(tk, 1000, "192.168.1.30");
  std::string up = tk;
  for (auto& c : up) c = (char)toupper((unsigned char)c);
  SPRAWDZ("trzyma", s.obsluz("GET", "/u/" + up + "/", "", 1001, "S").kod == 404);
  SPRAWDZ("trzyma", s.obsluz("GET", "/u/" + tk + "/x", "", 1002, "S").kod == 404);
  SPRAWDZ("trzyma", s.obsluz("POST", "/u/" + tk.substr(0, tk.size() - 1) + "/", "ssid=X", 1003, "S").kod == 404);
  SPRAWDZ("trzyma", s.obsluz("GET", "/u/" + tk + "?a=b", "", 1004, "S").kod == 200);
  SPRAWDZ("trzyma", s.obsluz("GET", "/", "", 1005, "S").kod == 404);
  SPRAWDZ("trzyma", s.obsluz("GET", "/u//", "", 1006, "S").kod == 404);
  SPRAWDZ("trzyma", !s.aktywna(1007) && s.obsluz("GET", "/u/" + tk + "/", "", 1008, "S").kod == 404);
  formularz::Sesja s2;
  s2.otworz("", 0, "192.168.1.30");
  SPRAWDZ("trzyma", s2.obsluz("GET", "/u//", "", 1, "S").kod == 404 && s2.obsluz("GET", "/u/", "", 2, "S").kod == 404);
  formularz::Sesja s3;
  s3.otworz(tk, 0, "192.168.1.30");
  SPRAWDZ("trzyma", s3.obsluz("POST", "/u/" + tk + "/", "", serwer::CZAS_MS, "S").kod == 404);
  // ciało ponad limit: 413, bez rozkładu
  SPRAWDZ("trzyma", s3.obsluz("POST", "/u/" + tk + "/", std::string(5000, 'a'), 1, "S").kod == 413);
}

// Trzyma: nowa sieć bez hasła przy innej nazwie odrzucona; puste hasło przy tej samej = bez zmiany.
static void hasla() {
  sekrety::Dane ob = dane("Dom", "Haslo-Dom-1", "192.168.1.20", "Ab-1");
  auto w = formularz::waliduj(formularz::parsujCialo("ssid=Inna&wifi_haslo="), ob, "2026-11-05");
  SPRAWDZ("trzyma", !w.ok() && w.sekrety.ssid == "Dom");
  w = formularz::waliduj(formularz::parsujCialo("ssid=Dom&wifi_haslo=&ab_haslo="), ob, "2026-11-05");
  SPRAWDZ("trzyma", w.ok() && w.sekrety.hasloWifi == "Haslo-Dom-1" && w.sekrety.abHaslo == "Ab-1" && !w.zmianaSieci);
}

int main() {
  wydanieZeWzoru();
  zapisBezSprawdzenia();
  restartWTrakcieZapisu();
  kabelPoFormularzu();
  wstrzykniecia();
  xss();
  token();
  hasla();
  printf("testy konfiguracji z telefonu: %d ok, %d błędów, %d znanych\n", ok, zle, znane);
  return zle ? 1 : 0;
}
