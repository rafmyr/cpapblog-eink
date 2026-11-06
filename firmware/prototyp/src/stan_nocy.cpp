// SPDX-License-Identifier: GPL-3.0-or-later
// Stan ostatniej nocy przy pobieraniu (spec 5.1.4): src/stan_nocy.h.
#include "stan_nocy.h"


#include "wspolne.h"

namespace stan_nocy {

const char* nazwa(Stan s) {
  switch (s) {
    case Stan::NIEOBECNA: return "nieobecna";
    case Stan::BRAK_DANYCH: return "brak_danych";
    case Stan::W_BUDOWIE: return "w_budowie";
    case Stan::W_BUDOWIE_STATUS: return "w_budowie_status";
    case Stan::KOMPLETNA: return "kompletna";
    case Stan::CZESCIOWA: return "czesciowa";
    case Stan::BEZ_TERAPII: return "bez_terapii";
  }
  return "nieobecna";
}

FazaNocy przejdz(const FazaNocy& s0, const std::string& dzis, int minuta, Obserwacja o, const Noc* rekord) {
  FazaNocy s = s0;
  if (s.data != dzis) s = FazaNocy{dzis, Stan::NIEOBECNA, false, false};
  if (dzis.empty()) return s;
  // rekord w historii rozstrzyga (zapis przed odświeżeniem, R-DANE-04; „częściowa” R-DANE-15, R-DANE-25)
  if (rekord != nullptr) {
    s.stan = rekord->czesciowa ? Stan::CZESCIOWA : Stan::KOMPLETNA;
    return s;
  }
  if (s.stan == Stan::KOMPLETNA || s.stan == Stan::CZESCIOWA) s.stan = Stan::NIEOBECNA;  // rekord zniknął (import)
  const bool po11 = minuta >= GODZINA_11, po2330 = minuta >= KONIEC_SPRAWDZEN;
  switch (o) {
    case Obserwacja::ZEGAR: break;
    case Obserwacja::BRAK_ODPOWIEDZI:
      // od 11:00 noc „nieobecna” bez odpowiedzi AirBridge to E-01, nie E-10 (R-DANE-20); w budowie i bez terapii
      // zostają w swoim stanie (brak odpowiedzi nic nie mówi o nocy)
      if (po11 && s.stan == Stan::NIEOBECNA) s.stan = Stan::BRAK_DANYCH;
      break;
    case Obserwacja::STATUS_BUDOWA:
      // R-DANE-35: tylko noc „nieobecna” albo „w budowie”, przed 23:30; bez terapii zostaje (choćby wieczorem rop = 1)
      if (!po2330 && (s.stan == Stan::NIEOBECNA || s.stan == Stan::BRAK_DANYCH || s.stan == Stan::W_BUDOWIE ||
                      s.stan == Stan::W_BUDOWIE_STATUS)) {
        if (s.stan != Stan::W_BUDOWIE && s.stan != Stan::W_BUDOWIE_STATUS) s.budowaPo11 = po11;
        s.stan = Stan::W_BUDOWIE_STATUS;
      }
      break;
    case Obserwacja::LISTA_BEZ_DNIA:
      // od 11:00, przy statusie bez stanu z R-DANE-35 (inaczej obserwacja byłaby STATUS_BUDOWA): bez terapii
      // (R-DANE-20; z „brak danych” E-01 znika, E-10); przed 11:00 noc zostaje „nieobecna” (R-DANE-22)
      if (po11 && (s.stan == Stan::NIEOBECNA || s.stan == Stan::BRAK_DANYCH ||
                   (s.stan == Stan::W_BUDOWIE_STATUS && !s.naLiscie)))
        s.stan = Stan::BEZ_TERAPII;
      else if (!po11 && s.stan == Stan::W_BUDOWIE_STATUS && !s.naLiscie)
        s.stan = Stan::NIEOBECNA;
      break;
    case Obserwacja::RAPORT_CZESCIOWY:
      // wejście w budowę; z „bez terapii” cofnięcie E-10 (R-DANE-21); moment wejścia decyduje o R-DANE-15 / R-DANE-25
      if (s.stan != Stan::W_BUDOWIE && s.stan != Stan::W_BUDOWIE_STATUS) s.budowaPo11 = po11;
      s.stan = Stan::W_BUDOWIE;
      s.naLiscie = true;
      break;
    case Obserwacja::RAPORT_KOMPLETNY:
      s.stan = Stan::KOMPLETNA;  // rekord przychodzi z tym samym wybudzeniem; bez rekordu zostaje do następnego
      break;
  }
  // R-DANE-35 o 23:30: dnia nadal nie ma na liście → „nieobecna”, bez E-10 i bez rekordu
  if (po2330 && s.stan == Stan::W_BUDOWIE_STATUS && !s.naLiscie) s.stan = Stan::NIEOBECNA;
  return s;
}

bool bezTerapii(const FazaNocy& s, const std::string& dzis) {
  return !dzis.empty() && s.data == dzis && s.stan == Stan::BEZ_TERAPII;
}

bool czesciowaTeraz(const FazaNocy& s, const std::string& dzis, int minuta) {
  if (minuta < GODZINA_11) return false;
  bool po11Wejscie = s.data == dzis && s.budowaPo11 &&
                     (s.stan == Stan::W_BUDOWIE || s.stan == Stan::W_BUDOWIE_STATUS);
  return po11Wejscie ? minuta >= KONIEC_SPRAWDZEN : true;
}

bool pobranieO2330(const FazaNocy& s, const std::string& dzis, int minuta, const Noc* rekord) {
  if (rekord != nullptr || s.data != dzis || dzis.empty() || minuta < KONIEC_SPRAWDZEN) return false;
  return (s.stan == Stan::W_BUDOWIE && s.budowaPo11) || (s.stan == Stan::W_BUDOWIE_STATUS && s.naLiscie);
}

std::string zapisStanu(const FazaNocy& s) {
  std::string f;
  if (s.budowaPo11) f += 'p';
  if (s.naLiscie) f += 'l';
  return s.data + ";" + nazwa(s.stan) + ";" + f;
}

FazaNocy odczytStanu(const std::string& t) {
  FazaNocy s;
  size_t a = t.find(';');
  if (a == std::string::npos) return s;
  size_t b = t.find(';', a + 1);
  std::string d = t.substr(0, a), n = t.substr(a + 1, b == std::string::npos ? std::string::npos : b - a - 1);
  std::string f = b == std::string::npos ? "" : t.substr(b + 1);
  if (!dataOk(d)) return s;
  for (int i = 0; i <= (int)Stan::BEZ_TERAPII; i++)
    if (n == nazwa((Stan)i)) {
      s.data = d;
      s.stan = (Stan)i;
      s.budowaPo11 = f.find('p') != std::string::npos;
      s.naLiscie = f.find('l') != std::string::npos;
      return s;
    }
  return s;
}

}  // namespace stan_nocy
