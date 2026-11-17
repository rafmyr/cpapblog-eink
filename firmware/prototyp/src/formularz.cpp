// SPDX-License-Identifier: GPL-3.0-or-later
#include "formularz.h"

#include "aktualizacja.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <initializer_list>
#include <utility>

#include "czynniki.h"
#include "ladowarka.h"
#include "miasta.h"
#include "raport.h"
#include "slot_dzienny.h"
#include "wspolne.h"

namespace formularz {

const char* const KLUCZE_ELEMENTOW[ELEMENTY] = {"maska", "filtr"};
const char* const NAZWY_ELEMENTOW[ELEMENTY] = {"Maska", "Filtr"};

namespace {
bool znakSterujacy(const std::string& s) {
  for (char c : s)
    if ((unsigned char)c < 0x20 || c == 0x7f) return true;
  return false;
}
int hex(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return -1;
}
std::string dekoduj(const std::string& s) {
  std::string r;
  for (size_t i = 0; i < s.size(); i++) {
    if (s[i] == '+') r += ' ';
    else if (s[i] == '%' && i + 2 < s.size() && hex(s[i + 1]) >= 0 && hex(s[i + 2]) >= 0) {
      r += (char)(hex(s[i + 1]) * 16 + hex(s[i + 2]));
      i += 2;
    } else r += s[i];
  }
  return r;
}
std::string wartosc(const std::map<std::string, std::string>& m, const char* k, bool& jest) {
  auto i = m.find(k);
  jest = i != m.end();
  return jest ? i->second : "";
}
bool zaznaczone(const std::map<std::string, std::string>& m, const char* k) {
  auto i = m.find(k);
  return i != m.end() && !i->second.empty();
}
bool liczbaOkresu(const std::string& s, int& v, int maks = 60) {
  if (s.empty() || s.size() > 3) return false;
  v = 0;
  for (char c : s) {
    if (c < '0' || c > '9') return false;
    v = v * 10 + (c - '0');
  }
  return v >= 1 && v <= maks;
}
std::string pole(const char* typ, const char* nazwa, const std::string& v, const std::string& dodatki = "") {
  return std::string("<input type=\"") + typ + "\" name=\"" + nazwa + "\" id=\"" + nazwa + "\" value=\"" + html(v) +
         "\"" + dodatki + ">";
}
std::string blad(const std::map<std::string, std::string>& b, const char* k) {
  auto i = b.find(k);
  return i == b.end() ? "" : "<p class=\"b\">" + html(i->second) + "</p>";
}
std::string wybor(const char* nazwa, const std::string& v, const char* const (*opcje)[2], int n) {
  std::string s = std::string("<select name=\"") + nazwa + "\" id=\"" + nazwa + "\">";
  for (int i = 0; i < n; i++)
    s += std::string("<option value=\"") + opcje[i][0] + "\"" + (v == opcje[i][0] ? " selected" : "") + ">" +
         opcje[i][1] + "</option>";
  return s + "</select>";
}

const int CISZA_OD_DOMYSLNIE = 22 * 60, CISZA_DO_DOMYSLNIE = 8 * 60;  // 5.6.4: 22:00–08:00 [PRÓG]
const char* const CZYNNIKI_DOMYSLNIE = "KL";  // 5.13.7: bez wiersza katar lub infekcja i leki (te przy L-01)

std::string hhmm5(int m) {
  char b[16];
  snprintf(b, sizeof b, "%02d:%02d", (m / 60) % 24, m % 60);
  return b;
}
// `GG:MM` z pola time (przeglądarka wysyła 5 znaków; sekundy odrzucone, bo panel ich nie ma)
bool godzinaOk(const std::string& x, int& m) {
  if (x.size() != 5 || x[2] != ':') return false;
  for (size_t i : {0, 1, 3, 4})
    if (x[i] < '0' || x[i] > '9') return false;
  int g = (x[0] - '0') * 10 + (x[1] - '0'), mi = (x[3] - '0') * 10 + (x[4] - '0');
  if (g > 23 || mi > 59) return false;
  m = g * 60 + mi;
  return true;
}
std::string kodyCzynnikow(const std::map<std::string, std::string>& m) {  // z pól `cz_X`, w kolejności POZYCJE
  std::string k;
  for (auto& poz : POZYCJE)
    if (poz.bit != CZ_NIC && zaznaczone(m, (std::string("cz_") + poz.kod).c_str())) k += poz.kod;
  return k;
}
bool znaneKody(const std::string& k) {
  for (char c : k) {
    bool ok = false;
    for (auto& poz : POZYCJE)
      if (poz.bit != CZ_NIC && poz.kod == c) ok = true;
    if (!ok) return false;
  }
  return true;
}
// Wartość okresu z pliku do pola formularza: najwyżej MAKS_OKRESU_Z_PLIKU bajtów, cięcie przed bajtem kontynuacji
// UTF-8 (nie rozcina znaku). Wartość ≤ 3 znaków (każda poprawna) bez zmian; dłuższa zostaje dłuższa niż 3, czyli zła.
std::string przytnijOkres(const std::string& v) {
  if (v.size() <= MAKS_OKRESU_Z_PLIKU) return v;
  size_t n = MAKS_OKRESU_Z_PLIKU;
  while (n > 0 && ((unsigned char)v[n] & 0xC0) == 0x80) n--;
  return v.substr(0, n);
}
std::string opcjeWyboru(const char* nazwa, const std::string& v, std::initializer_list<std::pair<const char*, const char*>> o) {
  std::string s = std::string("<select name=\"") + nazwa + "\" id=\"" + nazwa + "\">";
  for (auto& x : o)
    s += std::string("<option value=\"") + x.first + "\"" + (v == x.first ? " selected" : "") + ">" + x.second +
         "</option>";
  return s + "</select>";
}
// Sekcja „Ekrany” (ADR-0045, R-KONF-60). Pola: `ekrany_start` (sekcja w formularzu), `kk_start` i `dk_start` (kolejność
// pokazana w formularzu, jak `cz_start`), `kk_{id}` i `dk_{id}` (miejsce 1…n z listy), `pokaz_{id}` (pole wyboru).
// Bez JavaScriptu każde miejsce wybiera się z listy; pozycja, której miejsce się zmieniło, idzie na to miejsce (w górę:
// przed stojącą tam, w dół: za nią), reszta zachowuje kolejność. Ze skryptem strzałki ▲ ▼ przestawiają wiersze
// i numerują miejsca od nowa, więc serwer dostaje spójną kolejność. Ekrany zawsze włączone nie mają pól; nazwa spoza
// listy w ciele POST (np. `pokaz_ostatnia_noc`) nic nie zmienia.
void nowaKolejnosc(const std::map<std::string, std::string>& m, const char* prefiks, const char* const* ids, int n,
                   const int* stara, int* nowa, std::map<std::string, std::string>& bledy) {
  struct Poz { int id, klucz, stare; };
  std::vector<Poz> poz;
  for (int i = 0; i < n; i++) {
    int id = stara[i], miejsce = i + 1;
    std::string nazwa = std::string(prefiks) + ids[id];
    auto it = m.find(nazwa);
    if (it != m.end()) {
      int v = 0;
      if (indeksZTekstu(it->second.c_str(), n + 1, v) && v >= 1) miejsce = v;
      else bledy[nazwa] = "Wybierz miejsce z listy.";
    }
    // klucz sortowania: 2 × miejsce, przesunięte o −1 (w górę) albo +1 (w dół) dla pozycji, której miejsce się zmieniło
    int k = 2 * miejsce + (miejsce < i + 1 ? -1 : miejsce > i + 1 ? 1 : 0);
    poz.push_back({id, k, i});
  }
  std::stable_sort(poz.begin(), poz.end(), [](const Poz& a, const Poz& b) { return a.klucz < b.klucz; });
  for (int i = 0; i < n; i++) nowa[i] = poz[(size_t)i].id;
}
}  // namespace

void walidujEkrany(const std::map<std::string, std::string>& m, Wynik& w) {
  if (m.find("ekrany_start") == m.end()) return;  // sekcji nie było w formularzu: układ bez zmian
  UkladEkranow st;  // kolejność pokazana w formularzu (pola ukryte); zła = domyślna, z błędem sekcji
  std::vector<std::string> bl;
  bool jest = false;
  wierszUkladu(KLUCZ_KOKPIT_KOLEJNOSC, wartosc(m, "kk_start", jest), st, bl);
  wierszUkladu(KLUCZ_DZIEN_KOLEJNOSC, wartosc(m, "dk_start", jest), st, bl);
  if (!bl.empty()) {
    w.bledy["ekrany_start"] = "Odśwież stronę i ustaw kolejność jeszcze raz.";
    return;
  }
  std::map<std::string, std::string> bledy;
  UkladEkranow u;
  int stara[RODZAJOW_PRZESTAWNYCH], nowa[RODZAJOW_PRZESTAWNYCH];
  for (int i = 0; i < STRON_PRZESTAWNYCH; i++) stara[i] = static_cast<int>(st.kokpit.kolejnosc[i]);
  nowaKolejnosc(m, "kk_", ID_STRONY, STRON_PRZESTAWNYCH, stara, nowa, bledy);
  for (int i = 0; i < STRON_PRZESTAWNYCH; i++) u.kokpit.kolejnosc[i] = static_cast<StronaPrzestawna>(nowa[i]);
  u.kokpit.sypialnia = zaznaczone(m, "pokaz_sypialnia");
  for (int i = 0; i < RODZAJOW_PRZESTAWNYCH; i++) stara[i] = static_cast<int>(st.dzien.kolejnosc[i]);
  nowaKolejnosc(m, "dk_", ID_DZIENNY, RODZAJOW_PRZESTAWNYCH, stara, nowa, bledy);
  for (int i = 0; i < RODZAJOW_PRZESTAWNYCH; i++) u.dzien.kolejnosc[i] = static_cast<RodzajDzienny>(nowa[i]);
  for (int i = 0; i < RODZAJOW_DZIENNYCH; i++) u.dzien.pokaz[i] = zaznaczone(m, (std::string("pokaz_") + ID_DZIENNY[i]).c_str());
  {  // R-KON-77: wyłącznik samego wygaszacza (`dzien.wygaszacz`, domyślnie `tak` = brak wiersza) i jego próg
    bool jestW = false, jestM = false;
    std::string wg = wartosc(m, "wygaszacz", jestW);
    if (jestW) {
      if (wg == "tak") w.plik["dzien.wygaszacz"] = "";
      else if (wg == "nie") w.plik["dzien.wygaszacz"] = "nie";
      else bledy["wygaszacz"] = "Wybierz z listy.";
    }
    std::string mn = wartosc(m, "wygaszacz_min", jestM);
    if (jestM && wg != "nie" && !bledy.count("wygaszacz")) {  // przy wyłączonym czas bez znaczenia: bez zmian
      int n = 0;
      if (mn.empty()) w.plik["dzien.wygaszacz_min"] = "";
      else if (!indeksZTekstu(mn.c_str(), WYGASZACZ_MIN_DO + 1, n) || n < WYGASZACZ_MIN_OD)
        bledy["wygaszacz_min"] = "Liczba minut od 30 do 720.";
      else w.plik["dzien.wygaszacz_min"] = n == WYGASZACZ_MIN_DOMYSLNIE ? "" : std::to_string(n);
    }
  }
  if (!bledy.empty()) {  // R-KONF-18: zła wartość odrzucona, poprzedni układ zostaje
    w.plik.erase("dzien.wygaszacz_min");
    w.plik.erase("dzien.wygaszacz");
    for (auto& kv : bledy) w.bledy[kv.first] = kv.second;
    return;
  }
  w.plik[KLUCZ_KOKPIT_KOLEJNOSC] = kolejnoscKokpituTekst(u.kokpit);
  w.plik[KLUCZ_KOKPIT_UKRYTE] = ukryteKokpituTekst(u.kokpit);
  w.plik[KLUCZ_DZIEN_KOLEJNOSC] = kolejnoscDniaTekst(u.dzien);
  w.plik[KLUCZ_DZIEN_UKRYTE] = ukryteDniaTekst(u.dzien);
}

std::string html(const std::string& s) {
  std::string r;
  for (char c : s) {
    if (c == '<') r += "&lt;";
    else if (c == '>') r += "&gt;";
    else if (c == '&') r += "&amp;";
    else if (c == '"') r += "&quot;";
    else if (c == '\'') r += "&#39;";
    else r += c;
  }
  return r;
}

std::map<std::string, std::string> parsujCialo(const std::string& c) {
  std::map<std::string, std::string> m;
  if (c.size() > MAKS_CIALA) return m;
  size_t i = 0;
  while (i <= c.size()) {
    size_t j = c.find('&', i);
    if (j == std::string::npos) j = c.size();
    std::string para = c.substr(i, j - i);
    if (!para.empty()) {
      size_t r = para.find('=');
      std::string k = dekoduj(para.substr(0, r)), v = r == std::string::npos ? "" : dekoduj(para.substr(r + 1));
      if (!k.empty()) m[k] = v;
    }
    i = j + 1;
  }
  return m;
}

std::string koduj(const std::string& s) {
  static const char H[] = "0123456789ABCDEF";
  std::string r;
  for (char c : s) {
    unsigned char u = (unsigned char)c;
    if ((u >= 'a' && u <= 'z') || (u >= 'A' && u <= 'Z') || (u >= '0' && u <= '9') || u == '-' || u == '_' ||
        u == '.' || u == '~')
      r += c;
    else {
      r += '%';
      r += H[u >> 4];
      r += H[u & 15];
    }
  }
  return r;
}

bool adresAirBridgeOk(const std::string& a) {
  if (a.empty() || a.size() > 64) return false;
  std::string host = a;
  size_t d = a.rfind(':');
  if (d != std::string::npos) {
    std::string port = a.substr(d + 1);
    host = a.substr(0, d);
    if (port.empty() || port.size() > 5) return false;
    long p = 0;
    for (char c : port) {
      if (c < '0' || c > '9') return false;
      p = p * 10 + (c - '0');
    }
    if (p < 1 || p > 65535) return false;
  }
  if (serwer::adresPrywatny(host)) return true;
  const std::string L = ".local";
  if (host.size() <= L.size() || host.compare(host.size() - L.size(), L.size(), L) != 0) return false;
  for (char c : host)
    if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' || c == '.'))
      return false;
  return host[0] != '.' && host[0] != '-';
}

Pola zPliku(const std::string& konf, const sekrety::Dane& s, bool zrodloOto, double szerSecrets, double dlSecrets) {
  Pola p;
  p.ssid = s.ssid;
  p.hasloWifi = !s.hasloWifi.empty();
  p.abAdres = s.abAdres;
  p.abKonto = s.abKonto;
  p.hasloAb = !s.abHaslo.empty();
  std::string t = bezBom(konf), staryFiltr;
  bool jestTyg = false;
  size_t i = 0;
  while (i < t.size()) {
    size_t j = t.find('\n', i);
    if (j == std::string::npos) j = t.size();
    std::string w = t.substr(i, j - i);
    i = j + 1;
    obetnijKoniec(w);
    if (w.empty() || w[0] == '#') continue;
    size_t r = w.find('=');
    if (r == std::string::npos) continue;
    std::string k = w.substr(0, r), v = w.substr(r + 1);
    if (k == "profil" && (v == "publiczny" || v == "prywatny")) p.profil = v;
    // wymiana.maska = późniejsza z dat `wymiana.maska` i starego `wymiana.poduszka`; okres filtra w tygodniach
    // (`okres.filtr_tyg`, a bez niego stary `okres.filtr` w miesiącach × 4)
    if (k == "wymiana.maska" && dataOk(v) && v > p.wymiana[0]) p.wymiana[0] = v;
    if (k == "wymiana.poduszka" && dataOk(v) && v > p.wymiana[0]) p.wymiana[0] = v;
    // data filtra w tej samej postaci co pozostałe daty (RRRR-MM-DD); inna postać = pole puste, jak po wyczyszczeniu
    // przez przeglądarkę (pole `date` nie przyjmuje innej postaci), więc licznik zmian nie liczy jej od wczytania
    if (k == "wymiana.filtr") p.wymiana[1] = dataOk(v) ? v : "";
    // okres: poprawna wartość ma najwyżej 3 cyfry; dłuższa (zła) wartość z ręcznie poprawionego pliku przycięta do
    // MAKS_OKRESU_Z_PLIKU bajtów (bez rozcinania znaku UTF-8), więc zostaje zła i nie powiększa strony bez granicy
    if (k == "okres.maska") p.okres[0] = przytnijOkres(v);
    if (k == "okres.filtr_tyg") { p.okres[1] = przytnijOkres(v); jestTyg = true; }
    if (k == "okres.filtr") staryFiltr = v;
    if (k == "aktualizacja_auto") p.aktualizacjaAuto = v == "tak" ? "tak" : "nie";  // zła wartość = `nie`
  }
  // R-DANE-66: ten sam parser co panel (src/ladowarka.h), zła wartość = `nie`
  p.ladowanieLimit = ladowarka::limitWKonfiguracji(konf) ? "tak" : "nie";
  int m = 0;
  if (!jestTyg && liczbaOkresu(staryFiltr, m)) p.okres[1] = std::to_string(std::min(104, 4 * m));
  // te same parsery co w panelu (parsujUstawieniaDnia, czynnikiZKonfiguracji), więc formularz pokazuje to, co
  // panel naprawdę stosuje; zła wartość w pliku = wartość domyślna
  UstawieniaDnia u = parsujUstawieniaDnia(konf);
  p.zrodloOto = zrodloOto;
  p.co2 = u.co2 == 0 ? "nie" : "tak";
  p.humor = u.humor == 0 ? "wylaczony" : u.humor == 1 ? "oszczedny" : "normalny";
  p.buzzer = u.buzzer == 0 ? "nie" : "tak";
  p.podpowiedz = u.podpowiedz == 0 ? "nie" : "tak";
  if (u.ciszaOd >= 0 && u.ciszaDo >= 0) {
    p.ciszaOd = hhmm5(u.ciszaOd);
    p.ciszaDo = hhmm5(u.ciszaDo);
  }
  if (!u.miasto.empty()) {
    p.miasto = u.miasto;
  } else if (!std::isnan(szerSecrets) && !std::isnan(dlSecrets)) {  // współrzędne z secrets.h: najbliższe miasto w 0,05°
    for (int c = 0; c < miasta::MIASTA_N; c++)
      if (std::fabs(miasta::MIASTA[c].szer - szerSecrets) <= 0.05 && std::fabs(miasta::MIASTA[c].dl - dlSecrets) <= 0.05) {
        p.miasto = miasta::MIASTA[c].nazwa;
        break;
      }
  }
  bool ustawione = false;
  uint8_t cz = raport::czynnikiZKonfiguracji(konf, ustawione);
  p.czynniki = ustawione ? kody(cz) : CZYNNIKI_DOMYSLNIE;
  p.uwagaOMasce = std::to_string(u.uwagaOMasce > 0 ? u.uwagaOMasce : UWAGA_O_MASCE_DOMYSLNIE);  // R-KONF-42
  p.ekrany = u.ekrany;  // ADR-0045: ten sam parser co panel
  p.wygaszaczMin = std::to_string(u.wygaszaczMinSkuteczne());  // R-KON-74, R-KON-77
  p.wygaszacz = u.wygaszaczWlaczony() ? "tak" : "nie";
  return p;
}

Wynik waliduj(const std::map<std::string, std::string>& m, const sekrety::Dane& ob, const std::string& dzis) {
  Wynik w;
  w.sekrety = ob;
  bool jest = false;
  // sieć domowa (R-KONF-39: puste hasło przy tej samej nazwie = bez zmiany)
  std::string ssid = wartosc(m, "ssid", jest);
  if (jest) {
    std::string h = wartosc(m, "wifi_haslo", jest);
    bool bez = zaznaczone(m, "wifi_bez");
    if (ssid.empty() || ssid.size() > 32 || znakSterujacy(ssid)) {
      w.bledy["ssid"] = "Wpisz nazwę sieci (najwyżej 32 znaki).";
    } else if (!h.empty() && (h.size() < 8 || h.size() > 63 || znakSterujacy(h))) {
      w.bledy["wifi_haslo"] = "Hasło sieci: od 8 do 63 znaków.";
    } else if (h.empty() && !bez && ssid != ob.ssid) {
      w.bledy["wifi_haslo"] = "Nowa sieć: wpisz jej hasło albo zaznacz „bez hasła”.";
    } else {
      w.sekrety.ssid = ssid;
      w.sekrety.hasloWifi = !h.empty() ? h : bez ? "" : ob.hasloWifi;
    }
  }
  std::string ab = wartosc(m, "ab_adres", jest);
  if (jest) {
    if (!adresAirBridgeOk(ab)) w.bledy["ab_adres"] = "Adres w sieci domowej, np. 192.168.1.50 albo airbridge.local.";
    else w.sekrety.abAdres = ab;
  }
  std::string konto = wartosc(m, "ab_konto", jest);
  if (jest) {
    if (konto.size() > 32 || znakSterujacy(konto) || konto.find('"') != std::string::npos)
      w.bledy["ab_konto"] = "Konto: najwyżej 32 znaki, bez cudzysłowu.";
    else w.sekrety.abKonto = konto;
  }
  std::string abh = wartosc(m, "ab_haslo", jest);
  if (jest) {
    if (abh.size() > 64 || znakSterujacy(abh)) w.bledy["ab_haslo"] = "Hasło AirBridge: najwyżej 64 znaki.";
    else if (!abh.empty()) w.sekrety.abHaslo = abh;
    else if (zaznaczone(m, "ab_bez")) w.sekrety.abHaslo.clear();
  }
  w.zmianaSieci = w.sekrety.ssid != ob.ssid || w.sekrety.hasloWifi != ob.hasloWifi;
  w.zmianaAirBridge =
      w.sekrety.abAdres != ob.abAdres || w.sekrety.abKonto != ob.abKonto || w.sekrety.abHaslo != ob.abHaslo;
  // ustawienia z pliku (5.6.4)
  std::string v = wartosc(m, "profil", jest);
  if (jest) {
    if (v == "publiczny" || v == "prywatny") w.plik["profil"] = v;
    else w.bledy["profil"] = "Wybierz z listy.";
  }
  for (int e = 0; e < ELEMENTY; e++) {
    // okres filtra w tygodniach pod kluczem `okres.filtr_tyg` (stary `okres.filtr` w miesiącach zostaje czytany
    // przez panel tylko bez nowego klucza, więc zapis pola usuwa stary wiersz)
    std::string kw = std::string("wymiana.") + KLUCZE_ELEMENTOW[e];
    std::string ko = e == 0 ? "okres.maska" : "okres.filtr_tyg";
    v = wartosc(m, kw.c_str(), jest);
    if (jest) {
      bool zapis = false;
      if (v.empty()) { w.plik[kw] = ""; zapis = true; }
      else if (!dataOk(v) || nrDnia(v) < 0) w.bledy[kw] = "Data w postaci RRRR-MM-DD.";
      else if (dataOk(dzis) && v > dzis) w.bledy[kw] = "Data z przyszłości.";  // R-KONF-34
      else { w.plik[kw] = v; zapis = true; }
      if (zapis && e == 0) w.plik["wymiana.poduszka"] = "";  // stara data poduszki scalona z maską, wiersz znika
    }
    v = wartosc(m, ko.c_str(), jest);
    if (jest) {
      int n = 0;
      const int maks = e == 0 ? 60 : 104;
      if (v.empty()) { w.plik[ko] = ""; if (e == 1) w.plik["okres.filtr"] = ""; }
      else if (!liczbaOkresu(v, n, maks)) w.bledy[ko] = e == 0 ? "Liczba miesięcy od 1 do 60." : "Liczba tygodni od 1 do 104.";
      else { w.plik[ko] = std::to_string(n); if (e == 1) w.plik["okres.filtr"] = ""; }
    }
  }
  // R-MAS-16 (ADR-0041): `Nowy model maski od dziś` = data wymiany maski dziś (wygrywa z polem daty) i nowy model
  // u wołającego; bez daty panelu (zegar) pole odrzucone, bo zerowanie normy potrzebuje dnia
  // R-MAS-19: cofnięcie nowego modelu z dziś; oba pola naraz to sprzeczność, żadne nie działa
  if (zaznaczone(m, "nowy_model") && zaznaczone(m, "nowy_model_cofnij")) {
    w.bledy["nowy_model"] = "Zaznacz tylko jedno: nowy model albo cofnięcie.";
  } else if (zaznaczone(m, "nowy_model_cofnij")) {
    w.cofnijNowyModel = true;
  } else if (zaznaczone(m, "nowy_model")) {
    if (!dataOk(dzis)) {
      w.bledy["nowy_model"] = "Panel nie zna dzisiejszej daty. Spróbuj po synchronizacji czasu.";
    } else {
      w.plik["wymiana.maska"] = dzis;
      w.plik["wymiana.poduszka"] = "";
      w.bledy.erase("wymiana.maska");
      w.nowyModelMaski = true;
    }
  }
  // ustawienia z 5.6.4 dodane do formularza. Wartość domyślna = brak wiersza (R-KONF-20), więc przy niej
  // wiersz jest usuwany z pliku.
  v = wartosc(m, "co2", jest);
  if (jest) {
    if (v == "tak" || v == "nie") w.plik["co2"] = v == "tak" ? "" : "nie";  // ze źródłem domyślnie `tak`
    else w.bledy["co2"] = "Wybierz z listy.";
  }
  v = wartosc(m, "ladowanie_limit", jest);  // R-DANE-66: domyślnie `nie` = brak wiersza (R-KONF-20)
  if (jest) {
    if (v == "tak") w.plik["ladowanie_limit"] = "tak";
    else if (v == "nie") w.plik["ladowanie_limit"] = "";
    else w.bledy["ladowanie_limit"] = "Wybierz z listy.";
  }
  v = wartosc(m, "humor", jest);
  if (jest) {
    if (v == "normalny") w.plik["humor"] = "";
    else if (v == "wylaczony" || v == "oszczedny") w.plik["humor"] = v;
    else w.bledy["humor"] = "Wybierz z listy.";
  }
  v = wartosc(m, "buzzer", jest);
  if (jest) {
    if (v == "tak") w.plik["buzzer"] = "";
    else if (v == "nie") w.plik["buzzer"] = "nie";
    else w.bledy["buzzer"] = "Wybierz z listy.";
  }
  v = wartosc(m, "podpowiedz", jest);
  if (jest) {
    if (v == "tak") w.plik["podpowiedz"] = "";
    else if (v == "nie") w.plik["podpowiedz"] = "nie";
    else w.bledy["podpowiedz"] = "Wybierz z listy.";
  }
  {  // cisza nocna: obie godziny razem (początek różny od końca, R-BUZ-02)
    bool jestOd = false, jestDo = false;
    std::string co = wartosc(m, "cisza_od", jestOd), cd = wartosc(m, "cisza_do", jestDo);
    if (jestOd || jestDo) {
      int od = 0, doo = 0;
      bool odOk = jestOd && godzinaOk(co, od), doOk = jestDo && godzinaOk(cd, doo);
      if (!odOk) w.bledy["cisza_od"] = "Godzina w postaci GG:MM, np. 22:00.";
      if (!doOk) w.bledy["cisza_do"] = "Godzina w postaci GG:MM, np. 08:00.";
      if (odOk && doOk) {
        if (od == doo) w.bledy["cisza_do"] = "Początek i koniec ciszy muszą być różne.";
        else if (od == CISZA_OD_DOMYSLNIE && doo == CISZA_DO_DOMYSLNIE) w.plik["cisza"] = "";
        else w.plik["cisza"] = hhmm5(od) + "-" + hhmm5(doo);
      }
    }
  }
  {  // miasto do pogody i wschodu słońca: tylko z listy, nigdy zapytanie do internetu (ADR-0001)
    bool jestM = false;
    std::string mi = wartosc(m, "miasto", jestM);
    if (jestM) {
      if (mi.empty()) w.plik["miasto"] = "";  // nie wybrano: wpis znika, panel wraca do secrets.h (a bez niego Katowice)
      else if (miasta::znajdz(mi)) w.plik["miasto"] = mi;
      else w.bledy["miasto"] = "Wybierz miasto z listy.";
    }
  }
  {  // R-KONF-42 „Uwaga o masce od”: liczba całkowita 10–15 L/min; domyślne 10 usuwa wiersz (R-KONF-20)
    bool jestU = false;
    std::string u = wartosc(m, "uwaga_o_masce", jestU);
    if (jestU) {
      int n = 0;
      if (u.empty()) w.plik["uwaga_o_masce"] = "";
      else if (!indeksZTekstu(u.c_str(), UWAGA_O_MASCE_MAX + 1, n) || n < UWAGA_O_MASCE_MIN)
        w.bledy["uwaga_o_masce"] = "Liczba od 10 do 15.";
      else w.plik["uwaga_o_masce"] = n == UWAGA_O_MASCE_DOMYSLNIE ? "" : std::to_string(n);
    }
  }
  {  // sekcja „Aktualizacje” (ADR-0038): automat w pliku, token i hasło okna do NVS (wołający), nigdy w pliku
    bool jestA = false;
    std::string au = wartosc(m, "aktualizacja_auto", jestA);
    if (jestA) {
      if (au == "tak") w.plik["aktualizacja_auto"] = "tak";
      else if (au == "nie") w.plik["aktualizacja_auto"] = "";  // domyślnie `nie`: wiersz znika
      else w.bledy["aktualizacja_auto"] = "Wybierz z listy.";
    }
    bool jestK = false;
    std::string kl = wartosc(m, "klucz_wydan", jestK);
    if (jestK && !kl.empty()) {
      if (aktualizacja::tokenOk(kl)) w.kluczWydan = kl;
      else w.bledy["klucz_wydan"] = "Klucz: od 20 do 255 znaków, same litery, cyfry i znak _.";
    } else if (zaznaczone(m, "klucz_usun")) {
      w.usunKlucz = true;
    }
    bool jestH = false;
    std::string ho = wartosc(m, "haslo_okna", jestH);
    if (jestH && !ho.empty()) {
      if (ho.size() < sekrety::HASLO_OKNA_MIN_ZNAKOW || ho.size() > 64 || znakSterujacy(ho))
        w.bledy["haslo_okna"] = "Hasło okna: od 16 do 64 znaków.";
      else w.hasloOkna = ho;
    }
  }
  walidujEkrany(m, w);  // sekcja „Ekrany” (ADR-0045, R-KONF-60)
  {  // czynniki w raporcie dla lekarza (R-CZY-06): `cz_start` = stan pokazany w formularzu, zmiana tylko, gdy wybór jest inny
    bool jestS = false;
    std::string start = wartosc(m, "cz_start", jestS);
    if (jestS) {
      std::string nowy = kodyCzynnikow(m);
      if (!znaneKody(start)) w.bledy["cz_start"] = "Wybierz czynniki z listy.";
      else if (nowy != start) w.plik["raport.czynniki"] = nowy.empty() ? "N" : nowy;  // `N` = żaden czynnik
    }
  }
  return w;
}

std::string scal(const std::string& plik, const std::map<std::string, std::string>& zm) {
  std::string t = bezBom(plik), r;
  std::map<std::string, bool> zrobione;
  size_t i = 0;
  while (i < t.size()) {
    size_t j = t.find('\n', i);
    if (j == std::string::npos) j = t.size();
    std::string w = t.substr(i, j - i), czysty = w;
    i = j + 1;
    obetnijKoniec(czysty);
    size_t rr = czysty.find('=');
    std::string k = czysty.empty() || czysty[0] == '#' || rr == std::string::npos ? "" : czysty.substr(0, rr);
    auto z = k.empty() ? zm.end() : zm.find(k);
    if (z == zm.end()) {
      r += w + "\n";
      continue;
    }
    if (!zrobione[k] && !z->second.empty()) r += k + "=" + z->second + "\n";
    zrobione[k] = true;
  }
  for (auto& kv : zm)
    if (!zrobione[kv.first] && !kv.second.empty()) r += kv.first + "=" + kv.second + "\n";
  return r;
}

namespace {
// Sekcje strony U-03 w kolejności z makiety 14.11.2026 (R-KONF-59): numer sekcji pola, w którym walidacja zgłosiła
// błąd; -1 = pole spoza znanych sekcji (strona otwiera wtedy wszystkie sekcje, żeby błąd nie został ukryty).
const int SEKCJE = 9;  // 0 sieć, 1 AirBridge, 2 pogoda, 3 panel, 4 dźwięk i humor, 5 sprzęt, 6 raport, 7 aktualizacje,
                       // 8 ekrany (ADR-0045; na stronie zaraz po „Panel”)
int sekcjaPola(const std::string& k) {
  if (k == "ssid" || k == "wifi_haslo" || k == "wifi_bez") return 0;
  if (k.compare(0, 3, "ab_") == 0) return 1;
  if (k == "miasto") return 2;
  if (k == "profil" || k == "co2" || k == "ladowanie_limit") return 3;
  if (k == "humor" || k == "buzzer" || k == "cisza_od" || k == "cisza_do" || k == "podpowiedz") return 4;
  if (k.compare(0, 8, "wymiana.") == 0 || k.compare(0, 6, "okres.") == 0 || k.compare(0, 10, "nowy_model") == 0 ||
      k == "uwaga_o_masce")
    return 5;
  if (k.compare(0, 3, "cz_") == 0) return 6;
  if (k == "aktualizacja_auto" || k == "klucz_wydan" || k == "klucz_usun" || k == "haslo_okna") return 7;
  if (k.compare(0, 3, "kk_") == 0 || k.compare(0, 3, "dk_") == 0 || k.compare(0, 6, "pokaz_") == 0 || k == "ekrany_start" ||
      k == "wygaszacz" || k == "wygaszacz_min")
    return 8;
  return -1;
}
// Początek sekcji zwijanej: nagłówek z tytułem i bieżącą wartością (bez haseł i kluczy, R-KONF-38), treść w `div.w`.
std::string sekcja(const char* tytul, const std::string& wartosc, bool otwarta) {
  return std::string("<details") + (otwarta ? " open" : "") + "><summary><b>" + tytul + "</b><span>" + html(wartosc) +
         "</span></summary><div class=\"w\">";
}
const char* const KONIEC_SEKCJI = "</div></details>";
std::string dataPL(const std::string& d) {  // RRRR-MM-DD → DD.MM.RRRR (inna postać bez zmian)
  return dataOk(d) ? d.substr(8, 2) + "." + d.substr(5, 2) + "." + d.substr(0, 4) : d;
}
std::string liczbaCzynnikow(const std::string& kodyCz) {  // nagłówek sekcji „Raport dla lekarza”
  int n = 0;
  for (char c : kodyCz)
    for (auto& poz : POZYCJE)
      if (poz.bit != CZ_NIC && poz.kod == c) n++;
  if (n == 0) return "bez czynników";
  if (n == 1) return "1 czynnik";
  bool kilka = n % 10 >= 2 && n % 10 <= 4 && (n % 100 < 12 || n % 100 > 14);
  return std::to_string(n) + (kilka ? " czynniki" : " czynników");
}
// Licznik niezapisanych zmian (R-KONF-59): zapamiętuje stan pól przy uruchomieniu skryptu (po wczytaniu i ewentualnym
// oczyszczeniu wartości przez przeglądarkę) i liczy pola różne od niego; bez skryptu licznik zostaje ukryty, a
// formularz działa jak bez niego. Strona po odesłaniu z błędami (`data-b` na liczniku) pokazuje licznik dopiero po
// pierwszej zmianie pola. Pole z błędem przeglądarki w zwiniętej sekcji otwiera tę sekcję.
// Sekcja „Ekrany” (ADR-0045, R-KONF-60): w każdej liście `.l` skrypt dokłada do wiersza przyciski ▲ ▼ (44 px), które
// przestawiają wiersz i numerują pola „miejsce” od nowa; bez skryptu przycisków nie ma, a miejsce wybiera się z listy.
// Stan pól zapamiętany przy samym polu (`_p`), więc przestawienie wierszy nie myli licznika.
const char* const SKRYPT =
    "<script>(function(){var f=document.forms[0],n=document.getElementById('n'),e=f.elements,h=n.hasAttribute("
    "'data-b'),i;function w(x){return x.type=='checkbox'?x.checked:x.tagName=='SELECT'?x.selectedIndex:x.value}"
    "function z(){var k=0,x;for(i=0;i<e.length;i++){x=e[i];if(x.type!='hidden'&&x.type!='submit'&&x.type!='button'"
    "&&w(x)!==x._p)k++}n.textContent=k==0?'Brak niezapisanych zmian':k==1?'1 niezapisana zmiana':k+(k%10>1&&k%10<5&&"
    "(k%100<12||k%100>14)?' niezapisane zmiany':' niezapisanych zmian');n.hidden=h}function u(){h=false;z()}"
    "[].forEach.call(document.querySelectorAll('.l'),function(l){[].forEach.call(l.children,function(d){['\u25B2',"
    "'\u25BC'].forEach(function(t,k){var b=document.createElement('button');b.type='button';b.textContent=t;"
    "b.setAttribute('aria-label',k?'ni\u017Cej':'wy\u017Cej');b.onclick=function(){var o=k?"
    "d.nextElementSibling:d.previousElementSibling;if(!o)return;l.insertBefore(k?o:d,k?d:o);[].forEach.call("
    "l.children,function(r,j){r.querySelector('select').selectedIndex=j});u()};d.appendChild(b)})})});"
    "for(i=0;i<e.length;i++)e[i]._p=w(e[i]);f.addEventListener('input',u);f.addEventListener('change',u);"
    "f.addEventListener('invalid',function(v){var s=v.target.closest('details');if(s)s.open=true},true);z()})();"
    "</script>";
std::string odmianaEkranow(int n) {  // `1 ekran dzienny`, `3 ekrany dzienne`, `5 ekranów dziennych`
  if (n == 1) return "1 ekran dzienny";
  bool kilka = n % 10 >= 2 && n % 10 <= 4 && (n % 100 < 12 || n % 100 > 14);
  return std::to_string(n) + (kilka ? " ekrany dzienne" : " ekranów dziennych");
}
// Pole „miejsce” wiersza: lista 1…n z bieżącym miejscem (etykieta `miejsce` obejmuje listę; `</option>` pominięte,
// HTML 5 na to pozwala, strona krótsza).
std::string miejsce(const std::string& nazwa, int biezace, int n) {
  std::string s = "<label class=\"m\">miejsce <select name=\"" + nazwa + "\">";
  for (int i = 1; i <= n; i++) s += "<option" + std::string(i == biezace ? " selected>" : ">") + std::to_string(i);
  return s + "</select></label>";
}
std::string poleWyboru(const std::string& nazwa, bool zaznaczone, const char* opis) {
  return "<label class=\"c\"><input type=\"checkbox\" name=\"" + nazwa + "\" value=\"1\"" +
         (zaznaczone ? " checked" : "") + "> " + opis + "</label>";
}
// Sekcja „Ekrany” (ADR-0045, R-KONF-60): dwie listy `.l` z wierszami `.p` (pole „pokazuj” albo nazwa z „zawsze”,
// opis, miejsce), pod listami ekrany bez miejsca i to, co zawsze włączone. Wszystkie napisy stałe (bez danych z pliku).
std::string sekcjaEkranow(const Pola& p, const std::map<std::string, std::string>& b, bool otwarta) {
  const UkladKokpitu& k = p.ekrany.kokpit;
  const UkladDnia& d = p.ekrany.dzien;
  // nagłówek liczy strony z numerem, które naprawdę mogą być w kokpicie: „Ostatnia noc”, „7 nocy”
  // i „Ustawienia” zawsze, „Sypialnia · 30 nocy” tylko z odczytami z 30 nocy (formularz ich nie zna), więc „do 4 stron”
  std::string s = sekcja("Ekrany", std::string(k.sypialnia ? "do 4 stron" : "3 strony") + " · " + odmianaEkranow(d.wlaczonych()),
                         otwarta);
  s += "<p class=\"s\">Ekranów, które ostrzegają albo czekają na odpowiedź, nie da się wyłączyć. Ekrany rano "
       "zostają w tej samej kolejności.</p><p><b>Strony kokpitu</b></p><div class=\"l\">";
  // prostym językiem, bez nazw wewnętrznych: dlaczego tej strony nie da się ukryć
  static const char* const OPIS_STRONY[STRON_PRZESTAWNYCH] = {
      "zawsze widoczna (nie da się ukryć): tu jest ocena nocy i pytanie „Co było inaczej?”",
      "zawsze widoczna (nie da się ukryć): tu są uwagi, których nie ma rano na ekranie, i raport dla lekarza (▶ 2 s)",
      "tylko gdy są odczyty z sypialni z 30 nocy"};
  std::string start;
  for (int i = 0; i < STRON_PRZESTAWNYCH; i++) {
    int id = static_cast<int>(k.kolejnosc[i]);
    std::string n = std::string("kk_") + ID_STRONY[id];
    start += (i ? "," : "") + std::string(ID_STRONY[id]);
    s += "<div class=\"p\">" +
         (id == static_cast<int>(StronaPrzestawna::SYPIALNIA) ? poleWyboru("pokaz_sypialnia", k.sypialnia, NAZWA_STRONY[id])
                                                               : std::string("<b class=\"c\">") + NAZWA_STRONY[id] + "</b>") +
         "<p class=\"s\">" + OPIS_STRONY[id] + "</p>" + miejsce(n, i + 1, STRON_PRZESTAWNYCH) + blad(b, n.c_str()) + "</div>";
  }
  s += "</div><p class=\"s\"><b>Ustawienia</b> zawsze na końcu. Po „7 nocy” według swoich reguł: „Co znaczą te "
       "liczby” (profil publiczny) i „Otwarte sprawy” (gdy jest sprawa).</p><input type=\"hidden\" name=\"ekrany_start\" "
       "value=\"1\"><input type=\"hidden\" name=\"kk_start\" value=\"" + start + "\"><p><b>Ekrany w ciągu dnia</b></p>"
       "<p class=\"s\">Zawsze przed listą: ekran z rana ze sprawą (pytanie, brak danych, lekarz, problem z nocy), "
       "przeciek w drzemce i „Przewietrz teraz” (ekrany o CO2 w „Panel”). Ostrzeżenia i bateria są na każdym "
       "ekranie.</p><div class=\"l\">";
  static const char* const OPIS[RODZAJOW_PRZESTAWNYCH] = {"kiedy otworzyć okno", "ślisko, mróz, deszcz, wiatr",
                                                         "termin maski i filtra", "rodzaje z wydania panelu"};
  start.clear();
  for (int i = 0; i < RODZAJOW_PRZESTAWNYCH; i++) {
    int id = static_cast<int>(d.kolejnosc[i]);
    std::string n = std::string("dk_") + ID_DZIENNY[id];
    start += (i ? "," : "") + std::string(ID_DZIENNY[id]);
    s += "<div class=\"p\">" + poleWyboru(std::string("pokaz_") + ID_DZIENNY[id], d.pokaz[id], NAZWA_DZIENNEGO[id]) +
         "<p class=\"s\">" + OPIS[id] + "</p>" + miejsce(n, i + 1, RODZAJOW_PRZESTAWNYCH) + blad(b, n.c_str()) + "</div>";
  }
  s += "</div><input type=\"hidden\" name=\"dk_start\" value=\"" + start + "\">" +
       poleWyboru("pokaz_dzien_i_noc", d.pokazuj(RodzajDzienny::DZIEN_I_NOC), NAZWA_DZIENNEGO[4]) +
       "<p class=\"s\">zachód słońca, gdy nic nie czeka</p>" +
       poleWyboru("pokaz_obrazek", d.pokazuj(RodzajDzienny::OBRAZEK), NAZWA_DZIENNEGO[5]) +
       "<p class=\"s\">zamiast „Dzień i noc” i jako wygaszacz. Bez ekranu dziennego zostaje ekran z rana.</p>" +
       // R-KON-77: wyłącznik samego wygaszacza (`dzien.wygaszacz`) i czas bezczynności obok; teksty krótkie, bo strona
       // musi się zmieścić w REZERWACJA_STRONY także ze wszystkimi błędami
       "<label for=\"wygaszacz\">Wygaszacz ekranu (ostrzeżeń nie zasłania)</label>" +
       opcjeWyboru("wygaszacz", p.wygaszacz, {{"tak", "włączony"}, {"nie", "wyłączony"}}) + blad(b, "wygaszacz") +
       "<label for=\"wygaszacz_min\">Wygaszacz po (minuty bez przycisku)</label>" +
       pole("number", "wygaszacz_min", p.wygaszaczMin, " min=\"30\" max=\"720\" maxlength=\"3\" inputmode=\"numeric\"") +
       blad(b, "wygaszacz_min") + blad(b, "ekrany_start") + KONIEC_SEKCJI;
  return s;
}
}  // namespace

std::string strona(const Pola& p, const std::string& akcja, const std::string& komunikat,
                   const std::map<std::string, std::string>& b) {
  static const char* const PROFILE[][2] = {{"prywatny", "prywatny"}, {"publiczny", "publiczny"}};
  // R-KONF-59: sekcja otwarta po odesłaniu, gdy ma pole z błędem; przy pierwszej konfiguracji (brak sieci, brak
  // AirBridge) ta sekcja otwarta od razu; przy wyłączonym dźwięku „Dźwięk i humor” otwarta (ostrzeżenie widoczne);
  // okres wymiany z pliku poza zakresem pola otwiera „Sprzęt” (inaczej przeglądarka zatrzymałaby zapis na polu,
  // którego nie widać)
  bool otw[SEKCJE] = {};
  for (auto& kv : b) {
    int n = sekcjaPola(kv.first);
    if (n >= 0) otw[n] = true;
    else
      for (bool& o : otw) o = true;
  }
  if (p.ssid.empty()) otw[0] = true;
  if (p.abAdres.empty()) otw[1] = true;
  if (p.buzzer == "nie") otw[4] = true;  // ostrzeżenie o wyłączonym dźwięku widoczne bez dotykania
  for (int e = 0; e < ELEMENTY; e++) {
    int n = 0;
    if (!p.okres[e].empty() && !liczbaOkresu(p.okres[e], n, e == 0 ? 60 : 104)) otw[5] = true;
  }
  std::string s;
  s.reserve(REZERWACJA_STRONY);  // jedna rezerwacja zamiast kolejnych powiększeń przy składaniu (pamięć ESP32)
  s += "<!doctype html><html lang=\"pl\"><head><meta charset=\"utf-8\"><meta name=\"viewport\" "
       "content=\"width=device-width,initial-scale=1\"><meta name=\"color-scheme\" content=\"light\"><link "
       "rel=\"icon\" href=\"data:,\"><title>Ustawienia panelu</title><style>:root{color-scheme:light}html{scroll-padding-bottom:10em}"
       "body{font-family:system-ui,sans-serif;max-width:34em;margin:0 auto;color:#000;background:#fff}"
       "header{padding:1.2em 1em .8em;border-bottom:2px solid #000}header div{display:flex;flex-wrap:wrap;"
       "justify-content:space-between;align-items:baseline;gap:0 1em}h1{margin:0;font-size:1.4em}header p{margin:.3em 0 "
       "0;color:#444}details{border-bottom:1px solid #000}details[open]{background:#f2f2f2}summary{display:flex;"
       "align-items:center;gap:.75em;min-height:52px;box-sizing:border-box;padding:.4em 1em;cursor:pointer;list-style:"
       "none}summary::-webkit-details-marker{display:none}summary span{margin-left:auto;color:#444;font-size:.9em;"
       "text-align:right;overflow-wrap:anywhere}summary::after{content:\"\";flex:none;width:.5em;height:.5em;"
       "border:solid #000;border-width:0 2px 2px 0;transform:rotate(-45deg)}details[open]>summary::after{transform:"
       "rotate(45deg)}summary:focus-visible,button:focus-visible{outline:3px solid #000;outline-offset:-3px}.w{padding:0 "
       "1em 1em}label{display:block;margin:.8em 0 .2em;font-weight:bold}input,select{width:100%;min-height:46px;"
       "box-sizing:border-box;padding:.5em;font:inherit;font-size:1em;border:2px solid #000;border-radius:6px;color:"
       "#000;background:#fff}label.c{display:flex;align-items:center;gap:.6em;min-height:44px;margin:.2em 0;"
       "font-weight:normal}input[type=checkbox]{width:24px;height:24px;min-height:0;margin:0;flex:none}"
       ".b{margin:.3em 0;padding-left:.5em;border-left:4px solid #000;font-weight:bold}.s{color:#444;margin:.2em 0;"
       "font-size:.9em}.r{border:2px solid #000;padding:.6em;margin:1em}.z{position:sticky;bottom:0;background:#fff;"
       "border-top:2px solid #000;padding:.75em 1em 1em}.z p{margin:0 0 .5em;font-weight:bold}button{display:block;"
       "width:100%;min-height:52px;margin:0;padding:.8em;font:inherit;font-size:1.1em;border:2px solid #000;"
       "border-radius:6px;-webkit-appearance:none;appearance:none;background:#000;color:#fff;font-weight:bold}"
       ".p{display:flex;flex-wrap:wrap;align-items:center;gap:0 .5em;border-top:1px solid #000;padding:.3em 0}.p .c{"
       "flex:1 1 10em;margin:0}.p .s{flex-basis:100%;margin:0 0 .3em}.p .b{flex-basis:100%;order:1}.m{display:flex;align-items:center;gap:.4em;"
       "margin:0;font-weight:normal}.m select{width:auto}.p button{display:inline-block;width:44px;min-height:44px;"
       "padding:0;font-size:1em;background:#fff;color:#000}"
       "</style></head><body><header><div><h1>Ustawienia panelu</h1>";
  if (!p.wersja.empty()) s += "<span>wersja " + html(p.wersja) + "</span>";
  s += "</div><p>Te ustawienia dotyczą tylko panelu, nie aparatu.</p></header>";
  if (!komunikat.empty()) s += "<p class=\"r\">" + html(komunikat) + "</p>";
  s += "<form method=\"post\" action=\"" + html(akcja) + "\" autocomplete=\"off\">";
  s += sekcja("Sieć domowa", p.ssid.empty() ? "nie ustawiona" : p.ssid, otw[0]) +
       "<label for=\"ssid\">Nazwa sieci Wi-Fi</label>" +
       pole("text", "ssid", p.ssid, " maxlength=\"32\" autocapitalize=\"none\"") + blad(b, "ssid") +
       "<label for=\"wifi_haslo\">Hasło sieci</label>" +
       pole("password", "wifi_haslo", "",
            std::string(" maxlength=\"63\" autocomplete=\"new-password\"") +
                (p.hasloWifi ? " placeholder=\"ustawione\"" : "")) +
       blad(b, "wifi_haslo") + (p.hasloWifi ? "<p class=\"s\">Puste pole = hasło bez zmian.</p>" : "") +
       "<label class=\"c\"><input type=\"checkbox\" name=\"wifi_bez\" value=\"1\"> bez hasła</label>"
       "<p>Sieć domowa musi działać w paśmie 2,4 GHz.</p>" + KONIEC_SEKCJI;
  s += sekcja("AirBridge", p.abAdres.empty() ? "nie ustawiony" : p.abAdres, otw[1]) +
       "<label for=\"ab_adres\">Adres AirBridge</label>" +
       pole("text", "ab_adres", p.abAdres, " maxlength=\"64\" autocapitalize=\"none\" inputmode=\"url\"") +
       blad(b, "ab_adres") + "<p>Adres AirBridge znajdziesz w routerze, na liście urządzeń.</p>" +
       "<label for=\"ab_konto\">Konto AirBridge</label>" +
       pole("text", "ab_konto", p.abKonto, " maxlength=\"32\" autocapitalize=\"none\"") + blad(b, "ab_konto") +
       "<label for=\"ab_haslo\">Hasło AirBridge</label>" +
       pole("password", "ab_haslo", "",
            std::string(" maxlength=\"64\" autocomplete=\"new-password\"") +
                (p.hasloAb ? " placeholder=\"ustawione\"" : "")) +
       blad(b, "ab_haslo") + (p.hasloAb ? "<p class=\"s\">Puste pole = hasło bez zmian.</p>" : "") +
       "<label class=\"c\"><input type=\"checkbox\" name=\"ab_bez\" value=\"1\"> bez hasła</label>"
       "<p>Konto i hasło, którymi logujesz się do AirBridge.</p>" + KONIEC_SEKCJI;
  // miasto do pogody i wschodu słońca; lista z pamięci panelu, panel nie pyta internetu o miasto (ADR-0001)
  s += sekcja("Pogoda", p.miasto.empty() ? "nie wybrano" : p.miasto, otw[2]) +
       "<label for=\"miasto\">Miasto do pogody i wschodu słońca</label>"
       "<select name=\"miasto\" id=\"miasto\"><option value=\"\"" + std::string(p.miasto.empty() ? " selected" : "") +
       ">nie wybrano (ustawienie z wgrania panelu, jeśli jest)</option>";
  for (int i = 0; i < miasta::MIASTA_N; i++)
    s += std::string("<option") + (p.miasto == miasta::MIASTA[i].nazwa ? " selected" : "") + ">" +
         html(miasta::MIASTA[i].nazwa) + "</option>";
  s += "</select>" + blad(b, "miasto") +
       "<p class=\"s\">Panel nie pyta internetu o miasto: współrzędne ma w pamięci. Nowe miasto działa od następnego "
       "wybudzenia.</p>" + KONIEC_SEKCJI;
  s += sekcja("Panel", "profil " + p.profil, otw[3]) + "<label for=\"profil\">Profil publiczny</label>" +
       wybor("profil", p.profil, PROFILE, 2) + blad(b, "profil");
  if (p.zrodloOto)
    s += "<label for=\"co2\">Ekrany o CO2 w sypialni</label>" +
         opcjeWyboru("co2", p.co2, {{"tak", "tak"}, {"nie", "nie"}}) + blad(b, "co2") +
         "<p class=\"s\">Dotyczy ekranów z czujnika CO2. Wybór działa od następnego wybudzenia panelu.</p>";
  else
    s += "<p class=\"s\">Ten panel nie ma czujnika CO2.</p>";
  // R-DANE-66 (ADR-0046): limit ładowania na kablu, domyślnie wyłączony
  s += "<label for=\"ladowanie_limit\">Oszczędzanie baterii na kablu: ładuj do ok. 70%</label>" +
       opcjeWyboru("ladowanie_limit", p.ladowanieLimit, {{"nie", "nie"}, {"tak", "tak"}}) +
       blad(b, "ladowanie_limit") +
       "<p class=\"s\">Dla panelu, który stoi stale na kablu: bateria wolniej się zużywa. Po odłączeniu kabla ma ok. "
       "70%, nie 100%, więc wystarczy na krócej. Zmiana działa od następnego wybudzenia panelu.</p>";
  s += KONIEC_SEKCJI;
  s += sekcjaEkranow(p, b, otw[8]);  // ADR-0045, R-KONF-60
  s += sekcja("Dźwięk i humor", p.buzzer == "nie" ? "dźwięk wyłączony" : "cisza " + p.ciszaOd + "–" + p.ciszaDo, otw[4]) +
       "<label for=\"humor\">Humor na ekranie dnia</label>" +
       opcjeWyboru("humor", p.humor, {{"normalny", "normalny"}, {"oszczedny", "oszczędny"}, {"wylaczony", "wyłączony"}}) +
       blad(b, "humor") + "<label for=\"buzzer\">Dźwięk (buzzer)</label>" +
       opcjeWyboru("buzzer", p.buzzer, {{"tak", "włączony"}, {"nie", "wyłączony"}}) + blad(b, "buzzer") +
       "<p class=\"s\">Bez dźwięku nie usłyszysz sygnału o sprawie do lekarza.</p>" +
       (p.buzzer == "nie" ? "<p class=\"b\">Dźwięk jest wyłączony: sygnał o sprawie do lekarza (pilność A) nie zadzwoni.</p>"
                           : "") +
       "<label for=\"cisza_od\">Cisza nocna: od</label>" + pole("time", "cisza_od", p.ciszaOd) + blad(b, "cisza_od") +
       "<label for=\"cisza_do\">Cisza nocna: do</label>" + pole("time", "cisza_do", p.ciszaDo) + blad(b, "cisza_do") +
       "<p class=\"s\">W ciszy nocnej dźwięk czeka do jej końca. Zmiana działa od najbliższego ekranu dnia.</p>"
       "<label for=\"podpowiedz\">Podpowiedź „co do poprawy”</label>" +
       opcjeWyboru("podpowiedz", p.podpowiedz, {{"tak", "włączona"}, {"nie", "wyłączona"}}) + blad(b, "podpowiedz") +
       KONIEC_SEKCJI;
  s += sekcja("Sprzęt",
              p.wymiana[0].empty() ? "maska: brak daty" : "maska " + dataPL(p.wymiana[0]), otw[5]);
  for (int e = 0; e < ELEMENTY; e++) {
    std::string kw = std::string("wymiana.") + KLUCZE_ELEMENTOW[e], ko = e == 0 ? "okres.maska" : "okres.filtr_tyg";
    s += "<label for=\"" + kw + "\">";
    s += NAZWY_ELEMENTOW[e];
    s += ": wymieniona</label>";
    s += pole("date", kw.c_str(), p.wymiana[e]);
    s += blad(b, kw.c_str());
    if (!p.termin[e].empty()) s += "<p class=\"s\">" + html(p.termin[e]) + "</p>";  // R-MAS-17
    if (e == 0)  // R-MAS-16: po zapisie pole wraca do stanu niezaznaczonego (nie ma wartości w pliku)
      s += "<label class=\"c\"><input type=\"checkbox\" name=\"nowy_model\" value=\"1\"> Nowy model maski od dziś</label>" +
           blad(b, "nowy_model") +
           "<p class=\"s\">Zaznaczone pole ustawia datę wymiany maski na dziś (zamiast daty wyżej), a panel uczy się "
           "normy wycieku od nowa (14 nocy). Pomyłkę cofniesz tego samego dnia.</p>" +
           (p.nowyModelDzis ? std::string("<label class=\"c\"><input type=\"checkbox\" name=\"nowy_model_cofnij\" value=\"1\"> "
                                          "Cofnij nowy model z dziś</label><p class=\"s\">Wraca data wymiany maski i norma "
                                          "wycieku sprzed dzisiejszego zapisu.</p>")
                            : std::string());
    s += "<label for=\"" + ko + "\">";
    s += NAZWY_ELEMENTOW[e];
    s += e == 0 ? ": okres wymiany (miesiące, puste = domyślny)</label>" : ": okres wymiany (tygodnie, puste = domyślny)</label>";
    s += pole("number", ko.c_str(), p.okres[e], e == 0 ? " min=\"1\" max=\"60\" inputmode=\"numeric\"" : " min=\"1\" max=\"104\" inputmode=\"numeric\"");
    s += blad(b, ko.c_str());
  }
  // R-KONF-42: tylko wariant opisowy S-K-10 `Wyciek widoczny` (poniżej 15 L/min); S-K-10 „problem” od 15 nie zmienia
  s += "<label for=\"uwaga_o_masce\">Uwaga o masce od (wyciek, l/min)</label>" +
       pole("number", "uwaga_o_masce", p.uwagaOMasce, " min=\"10\" max=\"15\" inputmode=\"numeric\"") +
       blad(b, "uwaga_o_masce") +
       "<p class=\"s\">Od tej wartości do 15 l/min panel opisuje wyciek ekranem „Wyciek widoczny”, bez rady o masce. "
       "Domyślnie 10, 15 = bez tej informacji.</p>";
  s += KONIEC_SEKCJI;
  s += sekcja("Raport dla lekarza", liczbaCzynnikow(p.czynniki), otw[6]) +
       "<p>Czynniki nocy, o których ma wspominać raport:</p>";
  for (auto& poz : POZYCJE) {
    if (poz.bit == CZ_NIC) continue;
    s += std::string("<label class=\"c\"><input type=\"checkbox\" name=\"cz_") + poz.kod + "\" value=\"1\"" +
         (p.czynniki.find(poz.kod) != std::string::npos ? " checked" : "") + "> " + html(poz.etykieta) + "</label>";
  }
  s += "<input type=\"hidden\" name=\"cz_start\" value=\"" + html(p.czynniki) + "\">" + blad(b, "cz_start") +
       "<p class=\"s\">Bez zmiany raport podaje katar lub infekcję i leki tylko przy bezdechach centralnych. Po zapisie "
       "wybór obowiązuje zawsze.</p>" + KONIEC_SEKCJI;
  // sekcja „Aktualizacje” (przyjęta 09.11.2026); klucz jak pola haseł, w nagłówku tylko tryb i brak klucza
  s += sekcja("Aktualizacje",
              std::string(p.aktualizacjaAuto == "tak" ? "samodzielnie" : "ręcznie") + (p.kluczWydan ? "" : " · bez klucza"),
              otw[7]) +
       "<label for=\"aktualizacja_auto\">Instaluj nowe wersje samodzielnie</label>" +
       opcjeWyboru("aktualizacja_auto", p.aktualizacjaAuto, {{"nie", "nie"}, {"tak", "tak"}}) +
       blad(b, "aktualizacja_auto") +
       "<p class=\"s\">Panel instaluje sam tylko w dzień, poza terapią, przy naładowanej baterii. Raz na dobę pyta "
       "GitHub o nową wersję.</p>"
       "<label for=\"klucz_wydan\">Klucz wydań (token GitHub)</label>" +
       pole("password", "klucz_wydan", "",
            std::string(" maxlength=\"255\" autocomplete=\"off\" autocapitalize=\"none\"") +
                (p.kluczWydan ? " placeholder=\"ustawiony\"" : "")) +
       blad(b, "klucz_wydan") +
       "<label class=\"c\"><input type=\"checkbox\" name=\"klucz_usun\" value=\"1\"> usuń klucz</label>"
       "<p class=\"s\">Klucz dostajesz od osoby, która instalowała panel. Bez klucza panel nie pobiera aktualizacji.</p>"
       "<label for=\"haslo_okna\">Hasło okna serwisowego</label>" +
       pole("password", "haslo_okna", "",
            std::string(" maxlength=\"64\" autocomplete=\"new-password\"") +
                (p.hasloOkna ? " placeholder=\"ustawione\"" : "")) +
       blad(b, "haslo_okna") +
       "<p class=\"s\">Wgrywanie przez Wi-Fi z komputera w oknie serwisowym. Co najmniej 16 znaków; puste pole = bez "
       "zmiany.</p>" + KONIEC_SEKCJI;
  // R-KONF-59: przycisk przyklejony do dołu ekranu, nad nim licznik niezapisanych zmian (ukryty bez skryptu; po
  // odesłaniu z błędami walidacji `data-b`: ukryty do pierwszej zmiany pola)
  s += std::string("<div class=\"z\"><p id=\"n\" aria-live=\"polite\"") + (b.empty() ? "" : " data-b") +
       " hidden></p><button type=\"submit\">Zapisz na panelu</button></div>"
       "</form><p class=\"r\">Panel nie ostrzega o nagłych stanach. Przy duszności lub bólu w klatce wezwij pomoc.</p>";
  s += SKRYPT;
  s += "</body></html>";
  return s;
}

void Sesja::otworz(const std::string& t, uint32_t ms, const std::string& adres) {
  otwarta_ = true;
  od_ = ms;
  zle_ = 0;
  token_ = t;
  adres_ = adres;
}

bool Sesja::aktywna(uint32_t ms) const {
  return otwarta_ && !token_.empty() && zle_ < serwer::ZLE_MAKS && ms - od_ < serwer::CZAS_MS;
}

Odpowiedz Sesja::obsluz(const std::string& metoda, const std::string& pelna, const std::string& cialo, uint32_t ms,
                        std::string str) {
  Odpowiedz o;
  if (!aktywna(ms)) return o;
  std::string s = pelna.substr(0, pelna.find('?'));
  if (s == "/favicon.ico" || s.compare(0, 17, "/apple-touch-icon") == 0) return o;  // bez tokenu, bez licznika
  std::string tk;
  if (s.compare(0, 3, "/u/") == 0) {
    size_t k = s.find('/', 3);
    tk = s.substr(3, k == std::string::npos ? std::string::npos : k - 3);
    if (k != std::string::npos && k + 1 != s.size()) tk.clear();  // tylko `/u/{token}` i `/u/{token}/`
  }
  if (!serwer::rowneStalyCzas(tk, token_)) {
    zle_++;  // R-KONF-13, R-KONF-14
    return o;
  }
  if (metoda == "GET") {
    o.kod = 200;
    o.typ = "text/html; charset=utf-8";
    o.cialo = std::move(str);  // bez drugiej kopii strony
    return o;
  }
  if (metoda == "POST") {
    if (cialo.size() > MAKS_CIALA) {
      o.kod = 413;
      o.cialo = "Za duży formularz.";
      return o;
    }
    o.kod = 200;
    o.typ = "text/html; charset=utf-8";
    o.cialo.clear();
    o.zapis = true;
    o.pola = parsujCialo(cialo);
    return o;
  }
  o.kod = 405;
  o.cialo = "Tylko GET i POST.";
  return o;
}

}  // namespace formularz
