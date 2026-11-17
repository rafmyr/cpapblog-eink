// SPDX-License-Identifier: GPL-3.0-or-later
// Pamięć plików między przebiegami bez głębokiego snu (src/pliki_pamiec.h, ADR-0047, T-WYM-14).
#include "pliki_pamiec.h"

// Klucz pamięci: ścieżka bez ukośników na końcu („/dane/noce/” i „/dane/noce” to ten sam wpis; „/” zostaje „/”).
std::string PlikiZPamiecia::klucz(const char* nazwa) {
  std::string k = nazwa ? nazwa : "";
  while (k.size() > 1 && k.back() == '/') k.pop_back();
  return k;
}

void PlikiZPamiecia::wyczysc() {
  tresc_.clear();
  istnieje_.clear();
  rozmiar_ = 0;
}

// Unieważnia ścieżkę, jej plik tymczasowy, wszystko pod nią (usunięcie albo zmiana nazwy katalogu) i istnienie
// każdego katalogu nad nią (zapis mógł go założyć). Zmiana 2026-11-17.
void PlikiZPamiecia::niewazne(const char* nazwa) {
  const std::string k = klucz(nazwa);
  const std::string tmp = k + ".tmp", pod = k == "/" ? k : k + "/";
  auto pasuje = [&](const std::string& n) {
    return n == k || n == tmp || n.compare(0, pod.size(), pod) == 0;
  };
  for (auto t = tresc_.begin(); t != tresc_.end();) {
    if (pasuje(t->first)) {
      rozmiar_ -= t->second.size();
      t = tresc_.erase(t);
    } else {
      ++t;
    }
  }
  for (auto i = istnieje_.begin(); i != istnieje_.end();) {
    if (pasuje(*i)) i = istnieje_.erase(i);
    else ++i;
  }
  std::string g = k;
  for (;;) {
    size_t p = g.rfind('/');
    if (p == std::string::npos) break;
    g = g.substr(0, p);
    istnieje_.erase(g.empty() ? std::string("/") : g);
    if (g.empty()) break;
  }
}

bool PlikiZPamiecia::zamontuj() {
  bool przed = p_.zamontowane();
  bool w = p_.zamontuj();
  if (!przed || !w) wyczysc();  // nowe montowanie albo błąd: bez wiedzy o stanie nośnika
  return w;
}

// Pamiętane tylko „plik jest”: na panelu LittleFS.exists() otwiera plik (VFSFileImpl), więc false bywa też chwilowym
// błędem (brak pamięci na bufor przy Wi-Fi i TLS), nie tylko brakiem pliku. „Brak” zawsze sprawdzany na nośniku, jak
// po głębokim śnie.
bool PlikiZPamiecia::istnieje(const char* nazwa) {
  if (!wl_ || !p_.zamontowane()) return p_.istnieje(nazwa);
  const std::string k = klucz(nazwa);
  if (istnieje_.count(k) || tresc_.count(k)) return true;
  bool w = p_.istnieje(nazwa);
  if (w) istnieje_.insert(k);
  return w;
}

// Bez pamięci (zmiana 2026-11-17): adapter może zwrócić obcięty wiersz z true, a takiego
// wyniku nie da się odróżnić od poprawnego. Zawsze z nośnika, jak po głębokim śnie.
bool PlikiZPamiecia::pierwszyWiersz(const char* nazwa, std::string& wiersz) { return p_.pierwszyWiersz(nazwa, wiersz); }

bool PlikiZPamiecia::czytajWiersze(const char* nazwa, std::vector<std::string>& wiersze) {
  bool w = p_.czytajWiersze(nazwa, wiersze);  // może przywrócić `{nazwa}.tmp` (zmiana nazwy na nośniku)
  niewazne(nazwa);
  return w;
}

bool PlikiZPamiecia::zapiszAtomowo(const char* nazwa, const std::string& tresc) {
  niewazne(nazwa);
  bool w = p_.zapiszAtomowo(nazwa, tresc);
  niewazne(nazwa);
  return w;
}

bool PlikiZPamiecia::dopiszWiersz(const char* nazwa, const std::string& wiersz) {
  niewazne(nazwa);
  bool w = p_.dopiszWiersz(nazwa, wiersz);
  niewazne(nazwa);
  return w;
}

bool PlikiZPamiecia::usun(const char* nazwa) {
  niewazne(nazwa);
  bool w = p_.usun(nazwa);
  niewazne(nazwa);
  return w;
}

bool PlikiZPamiecia::zmienNazwe(const char* z, const char* na) {
  niewazne(z);
  niewazne(na);
  bool w = p_.zmienNazwe(z, na);
  niewazne(z);
  niewazne(na);
  return w;
}

void PlikiZPamiecia::odmontuj() {
  wyczysc();
  p_.odmontuj();
}

bool PlikiZPamiecia::czytaj(const char* nazwa, std::string& tresc) {
  if (!wl_ || !p_.zamontowane()) return p_.czytaj(nazwa, tresc);
  const std::string k = klucz(nazwa);
  auto t = tresc_.find(k);
  if (t != tresc_.end()) {
    trafien++;
    tresc = t->second;
    return true;
  }
  // Nieudany odczyt nigdy nie trafia do pamięci (ani jako treść, ani jako „brak pliku”): następny przebieg czyta
  // nośnik od nowa, dokładnie jak po głębokim śnie.
  odczytowNosnika++;
  bool w = p_.czytaj(nazwa, tresc);
  if (w && rozmiar_ + tresc.size() <= limit_) {
    tresc_[k] = tresc;
    rozmiar_ += tresc.size();
    istnieje_.insert(k);
  }
  return w;
}

size_t PlikiZPamiecia::zapisz(const char* nazwa, const std::string& tresc) {
  niewazne(nazwa);
  size_t w = p_.zapisz(nazwa, tresc);
  niewazne(nazwa);
  return w;
}

// Bez pamięci (zmiana 2026-11-17): lista katalogu zależy od każdego pliku w nim, a adapter
// na panelu może zwrócić listę częściową z true (pusty File z openNextFile() przy braku pamięci). Zawsze z nośnika.
bool PlikiZPamiecia::listuj(const char* katalog, std::vector<std::string>& nazwy) { return p_.listuj(katalog, nazwy); }

bool PlikiZPamiecia::formatuj() {
  wyczysc();
  bool w = p_.formatuj();
  wyczysc();
  return w;
}
