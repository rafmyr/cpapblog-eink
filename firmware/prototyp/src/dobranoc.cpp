// SPDX-License-Identifier: GPL-3.0-or-later
#include "dobranoc.h"
#include <cstdio>
#include "nawigacja.h"
#include "wspolne.h"

// Pula „noc” (5.7.5, pula przyjęta 08.11.2026): zdanie przypięte do sceny.
const std::vector<ZdanieSceny> ZDANIA_NOCY = {
    {"W12", "Dobranoc. Skargi przyjmę we śnie."},
    {"G14", "Kapcie zdjęte. Reszta świata niech poczeka."},
    {"G11", "Drzemka się przedłużyła. Nie wnoszę skargi."},
    {"G10", "Owce policzone. Jedna się nie zgadza."},
    {"G13", "Księżyc gra na zwłokę. Ja na sen."},
    {"G15", "Cisza wystąpi dziś bez dyrygenta."},
    {"G12", "Dziś nurkuję tylko w poduszce."},
};

// Sceny wariantów w kolejności rotacji (id jak w SCENY_NOCY, include/obrazy.h, tools/prototyp/generuj.py).
static const std::vector<const char*> SCENY_A[3] = {
    {"A1-W12", "A1-G14", "A1-G11"},
    {"A2-G10", "A2-G13", "A2-G15"},
    {"A3-G12", "A3-W12"},
};

// jedyne miejsce z listą scen
// spokojnych na ekran nocny. Poza listą: G11 (budzik łapie się za głowę, poci się i złości), G15 (energiczny dyrygent).
// Rotacja R-KON-32 liczy tylko sceny z tej listy.
const std::vector<const char*> SCENY_NOCY_DOZWOLONE = {"W12", "G14", "G10", "G13", "G12"};

static std::string idBezWariantu(const std::string& scena) {
  size_t k = scena.find('-');
  return k == std::string::npos ? scena : scena.substr(k + 1);
}

bool scenaNocyDozwolona(const std::string& scena) {
  std::string id = idBezWariantu(scena);
  for (auto d : SCENY_NOCY_DOZWOLONE)
    if (id == d) return true;
  return false;
}

std::vector<std::string> scenyNocyWariantu(WariantNocy w) {
  std::vector<std::string> r;
  for (auto s : SCENY_A[(int)w])
    if (scenaNocyDozwolona(s)) r.push_back(s);
  return r;
}

std::string wieczorNocy(const std::string& dzis, int m) {
  if (!dataOk(dzis) || !slotNocny(m)) return "";
  return m >= 23 * 60 + 30 ? dzis : dodajDni(dzis, -1);
}

long numerPoranka(const std::string& wieczor) {
  long n = nrDnia(wieczor);
  return n < 0 ? -1 : n + 1;
}

std::string zdanieNocy(const std::string& scena) {
  std::string id = idBezWariantu(scena);
  for (auto& z : ZDANIA_NOCY)
    if (id == z.scena) return z.zdanie;
  return "";
}

WyborNocy wyborNocy(long n, bool /*zPropozycjami*/, Humor humor) {
  WyborNocy w;
  if (n < 0) return w;
  w.n = n;
  int war = (int)(n % 3);
  w.wariant = (WariantNocy)war;
  if (humor == Humor::WYLACZONY) return w;  // R-KON-39: bez sceny i bez zdania, ta sama treść tekstowa
  std::vector<std::string> sc = scenyNocyWariantu(w.wariant);  // tylko sceny z listy dozwolonych
  if (sc.empty()) sc = {"A1-W12"};                              // zabezpieczenie; test pilnuje, że pula nie jest pusta
  w.scena = sc[(n / 3) % (long)sc.size()];
  w.zdanie = zdanieNocy(w.scena);  // pula „noc” przyjęta (08.11.2026): zdanie niezależnie od HUMOR_PROPOZYCJE
  return w;
}

DecyzjaNocy decyzjaNocy(const std::string& dzis, int minuta, bool ekranDnia, bool dzienTrwa, const StanNocy& s,
                        bool zPropozycjami, Humor humor) {
  DecyzjaNocy d;
  d.wieczor = wieczorNocy(dzis, minuta);
  if (d.wieczor.empty()) return d;  // dzień albo brak zegara
  if (s.wieczor == d.wieczor) {     // decyzja tej nocy już zapadła: S9 tylko, gdy powstał (R-KON-35, R-KON-37)
    d.nocny = s.s9 && ekranDnia;
  } else {  // pierwsze wybudzenie tej nocy: S9, gdy na ekranie jest ekran dnia, który nie musi zostać (R-KON-35)
    d.nowa = true;
    d.nocny = ekranDnia && !dzienTrwa;
    d.rysujPoZegarze = d.nocny;
  }
  if (d.nocny) d.wybor = wyborNocy(numerPoranka(d.wieczor), zPropozycjami, humor);
  return d;
}

StanNocy poDecyzji(const DecyzjaNocy& d, const StanNocy& s) {
  if (!d.nowa) return s;
  return {d.wieczor, d.nocny};
}

int maskaOd(const OdczytMaski& o) {
  return o.startSesji >= 0 && o.startSesji < 24 * 60 ? o.startSesji : -1;  // AirBridge v2.2.0+
}

DaneNocy daneNocy(const DecyzjaNocy& d, FaktyNocy& z, float temp, float wilg, int zwyklySen, int maskaOd) {
  if (d.wieczor != z.wieczor) {  // pierwsze rysowanie tej nocy (albo RTC po zaniku zasilania): fakty z tej chwili
    z = FaktyNocy();
    snprintf(z.wieczor, sizeof z.wieczor, "%s", d.wieczor.c_str());
    z.temp = temp;
    z.wilg = wilg;
    z.zwyklySen = (int16_t)zwyklySen;
    z.maskaOd = (int16_t)maskaOd;
  }
  DaneNocy n;
  n.wybor = d.wybor;
  n.wieczor = d.wieczor;
  n.temp = z.temp;
  n.wilg = z.wilg;
  n.zwyklySen = z.zwyklySen;
  n.maskaOd = z.maskaOd;
  return n;
}

std::string hhmm(int m) {
  if (m < 0) return "";
  char b[8];
  snprintf(b, sizeof b, "%02d:%02d", (m / 60) % 24, m % 60);
  return b;
}

static const char* const DNI_DOPELNIACZ[7] = {"niedzieli", "poniedziałku", "wtorku", "środy",
                                               "czwartku", "piątku", "soboty"};
static const char* const DNI_BIERNIK[7] = {"niedzielę", "poniedziałek", "wtorek", "środę",
                                           "czwartek", "piątek", "sobotę"};

std::string naglowekNocyA3(const std::string& wieczor) {
  int d = dzienTygodnia(wieczor);
  if (d < 0) return "Noc";
  return std::string(d == 3 ? "Noc ze " : "Noc z ") + DNI_DOPELNIACZ[d] + " na " + DNI_BIERNIK[(d + 1) % 7];
}

TekstyNocy tekstyNocy(const DaneNocy& d) {
  TekstyNocy t;
  bool czujnik = !std::isnan(d.temp) && !std::isnan(d.wilg);
  std::string temp = czujnik ? calkowita(d.temp) + "°" : "", wilg = czujnik ? calkowita(d.wilg) + "%" : "";
  // jedna nazwa odczytu czujnika panelu na całym panelu: `przy łóżku`
  std::string panel = czujnik ? "przy łóżku " + temp + " · " + wilg : "";
  std::string godz = hhmm(d.maskaOd);
  std::string zdanie = d.wybor.zdanie.empty() ? "" : "„" + d.wybor.zdanie + "”";
  switch (d.wybor.wariant) {
    case WariantNocy::A1:
      t.naglowekLewo = dataOk(d.wieczor) ? ddmm(d.wieczor) : "";  // data wieczoru bez dnia tygodnia (R-KON-33)
      if (!godz.empty()) t.naglowekLewo += " · maska od " + godz;
      // T12: prawa strona pusta (te same liczby stoją większe w faktach)
      t.etykieta = "DOBRANOC";
      t.tytul = "Śpij dobrze.";
      if (!godz.empty()) t.linie.push_back("Maska założona o " + godz + ".");
      t.linie.push_back("Wynik nocy pokażę rano.");
      if (czujnik) {
        t.fakty.push_back({temp, "temperatura"});
        t.fakty.push_back({wilg, "wilgotność"});
      }
      if (d.zwyklySen >= 0) t.fakty.push_back({godzH(d.zwyklySen), "zwykle z maską"});  // T13
      t.krok = "→ Ekran zostaje do rana, żeby nie błyskać.";  // T14: jedna linia
      t.stopkaLewo = zdanie;
      t.stopkaPrawo = "▶ kokpit";
      break;
    case WariantNocy::A2: {
      std::string l;
      if (!godz.empty()) l = "maska od " + godz;
      if (czujnik) l += (l.empty() ? "" : " · ") + panel;
      t.zdanieA2 = zdanie;
      t.liniaA2 = l + (l.empty() ? "" : " · ") + "wynik rano";
      t.tytul = "Dobranoc.";
      break;
    }
    case WariantNocy::A3:
      t.naglowekLewo = naglowekNocyA3(d.wieczor);
      t.naglowekPrawo = panel;
      t.tytulGodzina = !godz.empty();
      t.etykieta = t.tytulGodzina ? "MASKA ZAŁOŻONA O" : "NA DOBRANOC";
      t.tytul = t.tytulGodzina ? godz : "Dobranoc.";
      // bez godziny `Dobranoc.` jest już tytułem, więc zdanie bez powtórzenia
      t.linie.push_back(t.tytulGodzina ? "Dobranoc. Wynik nocy rano." : "Wynik nocy rano.");
      t.linie.push_back("Ekran zostaje do rana, żeby nie błyskać.");  // T15: to samo zdanie co A1
      t.stopkaLewo = zdanie;
      t.stopkaPrawo = "▶ kokpit";
      break;
  }
  return t;
}
