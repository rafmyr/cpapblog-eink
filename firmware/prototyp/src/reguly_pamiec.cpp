// SPDX-License-Identifier: GPL-3.0-or-later
// Pamięć reguł między przebiegami (src/reguly_pamiec.h, plik /dane/reguly.csv, format `reguly`).
#include <string>
#include <vector>

#include "magazyn.h"
#include "noc.h"  // czystePole()
#include "reguly_pamiec.h"
#include "reguly_pomoc.h"
#include "reguly_wewn.h"
#include "wspolne.h"

namespace reguly {
using namespace wewn;

// ---------- reguly_pamiec.h ----------
std::string kluczPamieci(const std::string& id, const std::string& wariant) { return kluczWyniku(id, wariant); }

Pamiec czytajPamiec(Magazyn& m) {
  Pamiec p;
  OdczytPliku o = m.czytaj(FORMAT_PAMIECI, SCIEZKA_PAMIECI);
  if (o.stan != StanPliku::DOBRY && o.stan != StanPliku::NOWSZY) return p;
  for (auto& w : o.wiersze) {
    std::vector<std::string> f = dziel(w, ';');
    if (f.size() < 8 || f[0].empty()) continue;
    int licznik = 0;
    if (!indeksZTekstu(f[4].c_str(), 1000000, licznik)) continue;
    PamiecReguly r;
    r.aktywnaOd = f[2];
    r.ostatnio = f[3];
    r.licznik = licznik;
    r.stanWzorca = f[5];
    r.wiem = f[6];
    r.omowione = f[7];
    p[kluczPamieci(f[0], f[1])] = r;
  }
  return p;
}

bool zapiszPamiec(Magazyn& m, const Pamiec& p) {
  std::vector<std::string> w;
  for (auto& kv : p) {
    size_t u = kv.first.find('/');
    std::string id = czystePole(kv.first.substr(0, u)), war = u == std::string::npos ? "" : czystePole(kv.first.substr(u + 1));
    const PamiecReguly& r = kv.second;
    const std::string pola[] = {war, czystePole(r.aktywnaOd), czystePole(r.ostatnio),
                                std::to_string(r.licznik < 0 ? 0 : r.licznik), czystePole(r.stanWzorca),
                                czystePole(r.wiem), czystePole(r.omowione)};
    std::string wiersz = id;
    for (const std::string& pole : pola) { wiersz += ';'; wiersz += pole; }
    w.push_back(wiersz);
  }
  return m.zapisz(FORMAT_PAMIECI, SCIEZKA_PAMIECI, w);
}

void aktualizujPamiec(Pamiec& p, const std::vector<Wynik>& wyniki, const std::string& dzis) {
  for (auto& w : wyniki) {
    if (!w.oceniona) continue;
    std::string kl = kluczPamieci(w.id, w.wariant);
    // przeglądu reguła wykluczona (R-EKR-48) trwa dalej, tylko bez ekranu
    bool trwa = w.spelniona || w.zmienne.count("spelniona_przed_wykluczeniem") > 0;
    if (trwa) {
      PamiecReguly& r = p[kl];
      if (r.aktywnaOd.empty()) r.aktywnaOd = dzis;
      if (r.ostatnio != dzis || r.licznik == 0) r.licznik++;
      r.ostatnio = dzis;
    } else {
      auto it = p.find(kl);
      if (it == p.end()) continue;
      it->second.aktywnaOd.clear();
      it->second.licznik = 0;
    }
  }
}

}  // namespace reguly
