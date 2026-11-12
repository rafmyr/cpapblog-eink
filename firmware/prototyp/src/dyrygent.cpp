// SPDX-License-Identifier: GPL-3.0-or-later
// Dyrygent wybudzenia (src/dyrygent.h, T-KOMP-01): przebieg jednego wybudzenia na adapterach z src/adaptery.h
// plus siatka aktualizacji (src/start.h). Decyzje woła z rdzenia (src/wybudzenie.h, src/start.h, src/cykl.h,
// src/airbridge.h, src/humor.h, src/otoczenie.h, …); tu zostaje kolejność kroków, składanie danych do ekranów
// i zapis plików.
//
// Cały stan jednego wybudzenia jest w obiekcie Przebieg (zmienne zwykłe prototypu); stan między wybudzeniami
// w StanRtc (Uruchomienie::rtc()), w NVS i w plikach, jak w prototypie.
#include "dyrygent.h"
#include <algorithm>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <map>
#include <set>
#include <utility>
#include <vector>
#include "airbridge.h"
#include "noc_pliki.h"  // noc z plików sesji (ADR-0036)
#include "portret.h"     // Portret nocy: przebieg ostatniej nocy (ADR-0040)
#include "portret_ekran.h"  // Portret nocy: rysunek (S11)
#include "porownanie.h"  // tryb porównawczy: noc z plików wobec raportu JSON (ADR-0036 „Zmiana 2026-11-09”)
#include "bateria.h"
#include "cykl.h"
#include "czynniki.h"
#include "odkrycia.h"      // odkrycia O-03…O-12
#include "podsumowanie.h"  // S7, S8, „Co znaczą te liczby”, ekran danych S1
#include "dobranoc.h"
#include "dzien.h"
#include "ekrany_kontekst.h"
#include "kontekst.h"
#include "obrazki.h"
#include "slot_dzienny.h"
#include "slot_wieczorny.h"   // okno slotu wieczornego, slot tygodniowy
#include "infekcja.h"
#include "kontekst_sk.h"
#include "sprzet.h"
#include "ekrany.h"
#include "humor.h"
#include "kokpit.h"
#include "kolejka.h"
#include "noc.h"
#include "otoczenie.h"
#include "plan.h"
#include "sprawy.h"
#include "stan_nocy.h"  // stany nocy 5.1.4
#include "stany.h"
#include "str.h"  // historia nocy ze STR.edf z AirBridge (ADR-0035)
#include "wzorce.h"  // stany wzorców L (5.12.12) i rodzaje warunków L i H
#include "raport.h"          // raport A4 dla lekarza
#include "raport_ekran.h"
#include "ekrany_konfiguracji.h"  // U-01, U-03
#include "formularz.h"           // formularz na telefonie
#include "sekrety.h"             // sieć i AirBridge w NVS
#include "aktualizacja.h"        // aktualizacja z wydań GitHub (ADR-0032, ADR-0038)
#include "serwer_raportu.h"
#include "sypialnia.h"
#include "wspolne.h"
// silnik reguł podpięty do ekranu dnia. CPAP_SILNIK_REGUL = 0 tylko do diagnozy (bez silnika:
// ekran dnia, linie i przyciski jak przed BETĄ); testy i symulator mogą podać wyniki przez sprawy::zrodloTestowe.
#ifndef CPAP_SILNIK_REGUL
#define CPAP_SILNIK_REGUL 1
#endif
#include "reguly_pamiec.h"
#include "reguly_pomoc.h"
#include "reguly_sygnaly.h"
namespace reguly {
void zarejestrujRodzajeE();  // src/rodzaje_e.cpp: rodzaje warunków zdarzeń E spoza rdzenia
}  // namespace reguly

// 1 (domyślnie w prototypie) = losuj też z listy propozycji pul zdań (5.7.5); 0 = tylko zdania przyjęte (R-HUM-27).
// Od puli przyjętej 08.11.2026 wszystkie zdania są przyjęte, a listy propozycji puste: flaga nie zmienia zdań.
// Flaga kompilacji z platformio.ini, jak w prototypie.
#ifndef HUMOR_PROPOZYCJE
#define HUMOR_PROPOZYCJE 1
#endif

namespace {

const bool Z_PROPOZYCJAMI = HUMOR_PROPOZYCJE != 0;

// Budżet czasu wybudzenia (README „Budżet czasu wybudzenia”) [stałe prototypu].
const uint32_t LIMIT_WIFI_MS = 20000;
// (R-KONF-10): nowa sieć z formularza ma 60 s [PRÓG] na połączenie, potem powrót do poprzedniej
const uint32_t LIMIT_NOWEJ_SIECI_MS = 60000;
// (R-KONF-08): hasło sieci panelu, 12 znaków [PRÓG] z alfabetu bez znaków mylących (0/o, 1/l/i), ok. 59 bitów
const int ZNAKI_HASLA_AP = 12;
// pauza po ostatniej odpowiedzi telefonowi przed wyłączeniem Wi-Fi (bufor TCP zdąży wyjść) [PRÓG]
const uint32_t PAUZA_PRZED_WYLACZENIEM_MS = 1500;
const uint32_t LIMIT_NTP_MS = 2000;  // odpowiedź SNTP w tym wybudzeniu, zegar znany [PRÓG]
// (R-AW-18): zegar nieznany (nowy panel, start po zaniku zasilania): 3 próby SNTP po 5 s, razem do 15 s [PRÓG].
// Koszt baterii: do 13 s Wi-Fi więcej niż przy limicie 2 s, ale tylko w wybudzeniu bez zegara; zwykłe wybudzenia
// (zegar znany) zostają przy jednej próbie 2 s. Bez zegara panel nie ocenia nocy, więc dłuższe czekanie jest tańsze
// niż wybudzenie co 30 min (R-AW-20) bez efektu.
const int NTP_PROBY_BEZ_ZEGARA = 3;
const uint32_t LIMIT_NTP_PROBY_BEZ_ZEGARA_MS = 5000;
const uint32_t AB_LIMIT_WYBUDZENIA_MS = 120000;
const uint32_t AB_LIMIT_ZALEGLYCH_MS = 90000;
// pliki danych przez magazyn (src/magazyn.h), formaty `pominiete` (doby bez rekordu, nie pobierane drugi raz)
// i `proby` (nieudane próby dób, airbridge.h ProbyDob) zamiast /ab_pominiete.txt i /ab_proby.txt prototypu.
// R-AW-16 (linia uwagi R-AW-05 przy pamięci plików, która się nie montuje, T-WYM-34).
const char* const TEKST_R_AW_16 = "Błąd pamięci panelu. Wyniki mogą być niepełne.";
// Linia zaproszenia do pytania „Co było inaczej?” (R-CZY-17, grupa 2a w 7.5)
const char* const TEKST_ZAPROSZENIA = "Co było inaczej? ↻ 2 s";
// false = panel tylko czyta /api/status, bez zapisu nocy (awaryjnie, np. do porównań)
const bool ZAPIS_NOCY_Z_AIRBRIDGE = true;
const uint32_t OBIEG_CZUWANIA_MS = 20;
// T-WYM-87 i R8 karty: okno przywracania przez port serwisowy po pierwszym formatowaniu albo przy starcie bez
// plików nocy; limit czekania na kolejny bajt treści komendy `W`; najdłuższy wiersz komendy z portu.
const uint32_t OKNO_PRZYWRACANIA_MS = 10UL * 60 * 1000;  // [szacunek] (spec 8.9 krok 4)
const uint32_t LIMIT_BAJTOW_W_MS = 5000;                 // [PRÓG] (R8)
// Limit całej treści jednej komendy `W` = LIMIT_BAJTOW_W_MS + tyle ms na każdy pełny KB
// (1 KB/s, przy 115 200 b/s port daje ok. 11 KB/s); w oknie przywracania najwyżej do końca okna [PRÓG]
const uint32_t LIMIT_W_NA_KB_MS = 1000;
// Strażnik zadania na czas pierwszego formatowania 23,9 MB (czas na egzemplarzu do zmierzenia) [PRÓG]
const uint32_t STRAZNIK_FORMATOWANIA_S = 600;
const size_t MAKS_WIERSZA_KOMENDY = 200;                 // `W` + nazwa + rozmiar + 64 znaki skrótu z zapasem

// Wynik pobrania jednej doby: BRAK_POLACZENIA i PRZERWANE kończą pobieranie w tym wybudzeniu.
enum class WynikDoby : uint8_t { ZAPISANA, CZEKA, NIEUDANA, E03, BRAK_POLACZENIA, PRZERWANE };

// Trwały stan pobierania historii: pominięte doby i nieudane próby.
struct StanHistAB {
  std::set<long> pom;
  ProbyDob proby;
  bool histZm = false, pomZm = false, probyZm = false;
};

struct PoNacisnieciu {
  bool zmiana = false;  // naciśnięcie coś zmieniło (inaczej w nocy bez przerysowania)
  Pobranie pobranie;    // pobranie po naciśnięciu, które zostało do zrobienia (ekran testu)
};

void kopiuj(char* cel, size_t rozmiar, const std::string& z) { snprintf(cel, rozmiar, "%s", z.c_str()); }

// Komplet adapterów z NVS podmienionym na warstwę z zapisem tylko przy zmianie (T-WYM-58).
Adaptery zNvs(const Adaptery& a, Nvs& n) {
  return Adaptery{a.zegar, n, a.pliki, a.siec, a.wyswietlacz, a.przyciski, a.czujniki, a.dzwiek, a.uruchomienie,
                  a.straznik, a.sen, a.port, a.diagnostyka, a.wydania};
}

// Skrót wierszy pliku (bez zmian treści = bez zapisu).
uint32_t skrotWierszy(const std::vector<std::string>& w) {
  std::string t;
  for (auto& x : w) t += x + "\n";
  return crc32((const uint8_t*)t.data(), t.size()) ^ (uint32_t)w.size();
}

// NOLINTNEXTLINE(clang-analyzer-optin.performance.Padding): jeden obiekt na wybudzenie, pola w kolejności prototypu
class Przebieg {
 public:
  Przebieg(const Adaptery& ad, const KonfiguracjaPanelu& ko, TekstyWybudzenia& t)
      : nvsZm(ad.nvs),
        a(zNvs(ad, nvsZm)),
        k(ko),
        r(ad.uruchomienie.rtc()),
        rej(ko.rejestr ? *ko.rejestr : rejestrFormatow()),
        mag(ad.pliki, [this](const std::string& w) { a.port.wiersz(w); }, rej),
        tk(t) {}

  // Jedno wybudzenie od startu programu do snu (dyrygent.h).
  void wykonaj() {
    // T-WYM-81: strażnik przed każdym innym adapterem
    UstawieniaStraznika u = ustawieniaStraznika();
    a.straznik.zapisz(u.limitS, u.panika);
    // T-WYM-82: decyzja przed montowaniem plików, migracjami i Wi-Fi
    PrzyczynaStartu przyczyna = a.uruchomienie.przyczyna();
    przyczynaStartu = przyczyna;
    wersjaId = identyfikatorWersji(a.uruchomienie.wersja());
    ZnacznikAwaryjny zn;
    parsujZnacznik(a.nvs.tekst(NVS_PRZESTRZEN, NVS_KLUCZ_AWARYJNY, ""), zn);
    DecyzjaStartu d = decyzjaStartu(a.uruchomienie.licznik(), przyczyna, wersjaId, zn, a.uruchomienie.stanWersji());
    log("start: przyczyna %s, starty po błędzie %u, spadki napięcia %u%s", nazwaPrzyczyny(przyczyna),
        (unsigned)d.poBledzie, (unsigned)d.spadki, d.spadekNapiecia ? " (start po spadku napięcia)" : "");
    if (d.usunZnacznik) {
      a.nvs.usun(NVS_PRZESTRZEN, NVS_KLUCZ_AWARYJNY);
      log("tryb awaryjny: start innej albo nowo wgranej wersji, znacznik usunięty (R-AW-39)");
    }
    PowodWybudzenia w = a.uruchomienie.powod();
    // skrót klatki tylko między wybudzeniami z uśpienia: po każdym innym starcie pierwsze rysowanie zawsze odświeża
    // e-ink
    if (przyczyna != PrzyczynaStartu::USPIENIE || w == PowodWybudzenia::START) r.klatkaOd = 0;
    if (d.awaryjny) {
      if (!trybAwaryjny(d, d.zapiszZnacznik ? d.znacznik : zn, w)) return;
      w = PowodWybudzenia::START;  // ↻ 2 s na U-06: zwykły start od nowa (R-AW-39)
    }
    zwykle = true;
    zwykleWybudzenie(w);
  }

  // teksty stanu tuż przed snem (TekstyWybudzenia, dyrygent.h): linie ekranu testu i kolejka linii uwagi. Tylko
  // po zwykłym wybudzeniu (tryb awaryjny pokazuje U-06 i U-05 bez tych linii); na panelu nie jest wołane (sen nie wraca).
  void tekstyKoncowe() {
    if (!zwykle) return;
    std::vector<std::string> l = tekstyTestu(daneTestu()), g = diagnostykaTestu();
    tk.linieTestu.insert(tk.linieTestu.end(), l.begin(), l.end());
    tk.diagnostykaTestu.insert(tk.diagnostykaTestu.end(), g.begin(), g.end());
    if (awariaPamieci()) tk.linieUwagi.push_back(TEKST_R_AW_16);
  }

 private:
  NvsTylkoZmiany nvsZm;  // przed `a`: `a.nvs` wskazuje na nią (kolejność inicjalizacji)
  Adaptery a;
  KonfiguracjaPanelu k;
  StanRtc& r;
  const std::vector<FormatPliku>& rej;
  Magazyn mag;
  TekstyWybudzenia& tk;
  bool zwykle = false;
  uint32_t wersjaId = 0;
  PrzyczynaStartu przyczynaStartu = PrzyczynaStartu::NIEZNANA;  // z wykonaj

  // magazyn danych (src/magazyn.h)
  std::set<std::string> bezZapisu;            // formaty czytane z pliku prototypu po nieudanej migracji (T-WYM-56)
  std::vector<std::string> bledyMigracji;     // `migracja {plik}: błąd` (ekran testu, T-WYM-56)
  std::vector<std::string> nvsNowsze;         // przestrzenie NVS z nowszej wersji (T-WYM-54)
  std::map<std::string, uint32_t> skrotPliku; // ścieżka → skrót wierszy ostatnio przeczytanych albo zapisanych
  std::set<int> wersjePlikow;                 // wersje z nagłówków przeczytanych plików (ekran testu)
  bool migracjeOk = true;                     // migracje na nowe ścieżki udane (samosprawdzenie, T-WYM-43)
  // układ partycji, pierwsze formatowanie, okno przywracania (T-WYM-84, T-WYM-87, T-WYM-62, R7)
  bool staryUklad = false;     // tablica partycji panelu ≠ wkompilowana (R7): bez formatowania, migracji i zapisu
  bool sformatowano = false;   // pierwsze formatowanie w tym starcie (T-WYM-84)
  bool oknoPrzywr = false;     // okno przywracania trwa (linia ekranu testu)
  uint32_t oknoPrzywrDoMs = 0; // ms końca okna przywracania
  Magazyn::WynikPrzywracania przywrocenie;  // ostatnie przywrócenie z `/przywracanie/` (ekran testu, log)
  // U2 wariant A: pliki zrzutu odrzucone w oknie przywracania i nie przyjęte później (NVS `cpap`/`przywr_odrz`);
  // przy niepustej liście udane przywrócenie nie stawia znacznika, więc okno wraca przy następnym starcie zasilania
  std::set<std::string> odrzucone;
  bool odrzuconeWczytane = false;

  // stan bieżącego cyklu (zmienne zwykłe, do ekranu testu)
  int httpKod = 0;
  uint32_t httpMs = 0;
  std::string abAdres = "-";
  std::string powod = "start";
  std::string powodBaterii = "start";  // pole `powod` w /bateria.csv (src/plan.h powodWpisuBaterii(), krok 2)
  PowodWybudzenia wybudzenieZ = PowodWybudzenia::START;  // powód tego wybudzenia (chwilaObsluzona())
  uint32_t obsluzonePrzyStarcie = 0;  // wybudzenie zegarem ze znanym czasem: chwila powodyTeraz
  std::string ostatniPrzycisk = "-";
  float temp = NAN, wilg = NAN, bateria = NAN;
  int procentBat = -1;  // M-25: procent baterii po wygładzeniu (src/bateria.h); -1 = brak odczytu napięcia
  bool bateriaZOdczytu = false;  // M-25 z napięcia zmierzonego w tym wybudzeniu (U-05 `Bateria: –` bez niego, R-KONF-41)
  bool wSilniku = false;         // trwa silnikDzis(): E-05 krytyczne jeszcze nie znane (e05KrytyczneTeraz())
  std::string wifiOpis;
  bool odczytUdany = false;
  bool wifiByloPolaczone = false;  // samosprawdzenie (T-WYM-41): połączenie w tym wybudzeniu
  bool wifiProbowano = false;      // była próba połączenia w tym wybudzeniu (samosprawdzenie nie łączy drugi raz)

  std::vector<Noc> hist;
  portret::Przebieg przebiegNocy;  // przebieg ostatniej nocy z `/dane/portret.csv` (Portret nocy, T-DANE-38)
  bool portretWczytany = false;
  portret::Przebieg portretDoZapisu;  // przebieg czekający na zapis po Wi-Fi
  bool portretCzeka = false;
  size_t portretPamiec = 0;
  bool drugiEkranTeraz = false;  // to rysowanie pokazuje drugi ekran poranka
  std::vector<WpisCzynnikow> czynniki;
  std::vector<WpisHumoru> dziennik;
  // wyniki silnika w tym wybudzeniu i ich skutki na ekranach (src/sprawy.h, src/kolejka.h)
  struct StanB5 {
    std::string dzis;                       // dla jakiej daty policzono ("" = jeszcze nie)
    std::vector<reguly::Wynik> wyniki;      // puste = silnika nie ma albo nic nie policzył
    reguly::Pamiec pamiec;                  // /dane/reguly.csv (format i zapis: src/reguly_pamiec.cpp)
    std::vector<sprawy::SprawaL> sprawy;
    std::string kluczEkranu;                // sprawa na ekranie dnia ("" = żadna)
    std::vector<kolejka::LiniaUwagi> linie;
    Stany stany;                            // stany(dzis) z oceną uspokojenia z wyników (5.4.13 pkt 1, 4, 7)
    kolejka::PytanieDnia pytanie;           // pytanie tak/nie, gdy jest ekranem dnia
    int karta = 1;                          // „Otwarte sprawy”: bieżąca karta
    std::string kluczWiem;                  // sprawa ostatniego „wiem” (ekran „Przeniesione”)
    sprawy::Odpowiedz ostatniaOdp;          // ostatnia odpowiedź (ekran „Zapisano”)
    bool pamiecWczytana = false;
    int termin = 11 * 60;                   // termin pytań porannych (R-PIE-13)
    reguly::Sygnaly sygnaly;                // sygnały komponentów ostatniej oceny
    std::set<std::string> wykluczone;       // noce wyłączone czynnikami (Kontekst::wykluczoneNoce)
    std::vector<WpisCzynnikow> zdjeteWyjazd;  // wpisy A zdjęte „nie” na R-CZY-22 (cofnięcie R-PRZ-11)
    std::vector<sprawy::SprawaMetryki> metryki;  // metryki zdrowotne w stanie odchylenia (R-NOR-21)
    std::vector<sprawy::LiniaZdarzenia> linieE;  // linie zdarzeń E (7.5)
  } b5;
  struct OdkrycieK1B15 { std::string dzien, tytul, powod; };  // R-CZY-20 (5.5.2 „Odkrycie o czynniku”)
  OdkrycieK1B15 k1OdkrycieB15;
  std::vector<sprawy::Odpowiedz> odpowiedzi;  // /dane/odpowiedzi.csv (5.12.8)
  int importWczytane = -1, importPominiete = 0;
  uint32_t ziarno = 0;
  bool otaAktywne = false;
  uint32_t otaDo = 0;  // ms końca okna (do napisu na ekranie testu)
  StanNocy stanNocy;   // ekran nocny: NVS „cpap”, klucze noc_w i noc_s9
  // stan ostatniej nocy przy pobieraniu (5.1.4, src/stan_nocy.h), NVS „cpap”, klucz sn_noc; obserwacja tego
  // wybudzenia z pobierz(), przejście w odczyt()
  stan_nocy::FazaNocy faza;
  bool fazaWczytana = false;
  stan_nocy::Obserwacja obsNocy = stan_nocy::Obserwacja::ZEGAR;
  std::vector<NocSypialni> sypNoce;
  bool otoTeraz = false;  // udana odpowiedź źródła otoczenia w tym wybudzeniu
  StanE08 stanE08;        // E-08 raz na epizod: NVS „cpap”, klucze e08_od, e08_d, e08_k
  std::vector<WpisBaterii> baterie;
  bool baterieWczytane = false;
  uint32_t wifiOdMs = 0;  // ms włączenia Wi-Fi w bieżącej sesji (0 = Wi-Fi wyłączone)
  uint32_t wifiLacznieMs = 0;  // czas z Wi-Fi w zamkniętych sesjach tego wybudzenia (krok 2: kilka sesji naraz)
  bool dzienOdswiezalny = false;  // ostatni rysujDzien(): ekran dnia ze stopką standardową R-EKR-10 (R-EKR-60)
  // wybudzenie zegarem tylko z powodu R-EKR-60 (przerysowanie bez pobrania): tryb i linia uwagi z RTC
  bool przerysowanie = false;
  // wybudzenie zegarem z Wi-Fi tylko na odczyt stanu aparatu (src/plan.h tylkoStatus, R-KON-42)
  bool tylkoStatusTeraz = false;
  // ocena slotu wieczornego (R-KON-01): tylko `/api/status` i źródło otoczenia, bez drzemki
  // (R-KON-57 to slot dzienny) i bez `GET /api/report`
  bool wieczorTeraz = false;
  bool prognozaTeraz = false;  // POWOD_PROGNOZA_GODZINOWA w tym wybudzeniu (R-KON-52)
  bool pobranieRano = false;   // R-PRZ-22: po rysowaniu akcji przycisku budzącego pobranie nocy (rano bez nocy)
  // wybudzenie zegarem tylko z oceną wieczorną bez Wi-Fi (ewentualnie z zapisem czujników): stopka
  // czujnika z ostatnio wyświetlonej klatki (StanRtc::stopkaTemp), więc e-ink tylko przy zmianie ekranu
  // Od 06.11.2026 także ocena slotu dziennego o pełnej godzinie (R-KON-42): stopka z odczytów
  // klatki, wybór ekranu ze świeżych; e-ink tylko, gdy zmienia się coś poza odczytami w stopce.
  bool stopkaZPamieci = false;
  // naciśnięcie odtwarzane z kolejki (R-PRZ-20): bez pisku w rdzeniu; pisk był w chwili naciśnięcia (adapter), a pisk
  // przy odtwarzaniu przychodziłby po rysowaniu i kilka naraz zlewało się w nieregularne sygnały
  bool zKolejkiTeraz = false;
  int skrotBudzenia = 0;  // przytrzymanieLubSkrot() przy przycisku budzącym (R-PRZ-21)
  float zaOknemKlatki = NAN;  // temperatura za oknem w stopce rysowanej klatki slotu dziennego (StanRtc::stopkaZaOknem)
  float tempStopki() const { return stopkaZPamieci && r.klatkaOd && !std::isnan(r.stopkaTemp) ? r.stopkaTemp : temp; }
  float wilgStopki() const { return stopkaZPamieci && r.klatkaOd && !std::isnan(r.stopkaWilg) ? r.stopkaWilg : wilg; }
  bool ekranWlaczony = false;  // sterownik ekranu włączony dopiero przed pierwszym rysowaniem (wybudzenie pomiarowe bez)
  // Stan pobierania historii jednej sesji Wi-Fi (krok 2, 7.1 krok 10): pobranie i zaległe doby piszą do niego, a zapis
  // plików idzie dopiero po wyłączeniu Wi-Fi, przed rysowaniem (zapiszOdlozone()).
  StanHistAB sh;
  bool shWczytany = false;
  bool odlozZapis = false;
  uint32_t abPoprzedni = 0;  // (R-CZY-19): chwila poprzedniej udanej odpowiedzi AirBridge przed tym odczytem

  WyzwanieDigest abWyzwanie;  // nonce z ostatniego 401, ważny do końca wybudzenia (albo do kolejnego 401)
  unsigned abNc = 0;
  uint32_t abKoniecMs = 0;  // ms końca budżetu żądań w tym wybudzeniu
  std::vector<DzienListy> abLista;
  bool abListaOk = false;
  std::set<long> abProbowane;  // doby z nieudaną próbą w tym wybudzeniu
  // STR.edf (ADR-0035): najwyżej jedna próba na wybudzenie; etag pobranego i przetworzonego pliku czeka na zapis
  // historii (zapiszStanHist()) i dopiero po nim trafia do NVS
  bool strProbowano = false;
  std::string strEtagCzeka;
  // STR.edf pobrany w tym wybudzeniu (ustawienia nocy z plików sesji i noce 'S'); w PSRAM do końca wybudzenia
  Str strWyb;
  bool strWybOk = false, strWybProbowano = false;
  std::string strWybEtag;    // etag STR z listy sesji odczytanej przed pobraniem strWyb ("" = nieznany)
  std::string strEtagListy;  // etag STR z ostatniej listy sesji doby (`/api/sessions?since=…`, blok `str`)
  // noc z plików sesji (ADR-0036): /api/sessions w tym wybudzeniu: -1 nie pytano, 0 niedostępne (404, błąd), 1 działa
  int sesjeAB = -1;
  porownanie::Przyczyna sesjeABPowod = porownanie::Przyczyna::BLAD_LISTY;  // powód sesjeAB == 0 (404 albo błąd listy)
  // tryb porównawczy (`noc_zrodlo=porownanie`, od 11.11.2026 tylko jawnie): wyniki z /dane/porownanie.csv, zapis odłożony jak historia
  std::vector<porownanie::Wpis> porWpisy;
  bool porWczytany = false, porZm = false;
  std::map<long, NocAB> porNoce;     // noce z raportu JSON tego wybudzenia czekające na porównanie (doba → noc)
  bool przyciskWCzuwaniu = false;    // czuwanie po rysowaniu obsłużyło przycisk: bez porównania na końcu
  std::string abPna, abSrn;    // `pna` i `srn` z `/api/status` tego wybudzenia ("" = brak odczytu)
  // wiersze plików nocy nieczytelne dla parsujNocV2, po ścieżce pliku; zapiszHistorie je oddaje
  std::map<std::string, std::vector<std::string>> nieczytelneNoce;
  bool abPrzerywalne = false;
  PrzerwaniePrzyciskiem abDetektor;
  bool abPrzerwano = false;
  // R-PRZ-22: pobranie rano po przycisku zostawia w kolejce naciśnięcie, które je przerwało (czuwanie wykona jego akcję)
  bool abZostawKolejke = false;
  bool abPobierano = false;  // to wybudzenie już pytało AirBridge (np. ↻ na S5 i S10): bez pobrania rano R-PRZ-22
  bool odczytPrzerwany = false;  // ostatni odczyt() przerwany przyciskiem (R-PRZ-22: bez liczenia limitu 2 min)
  uint8_t zaciete = 0;  // U4: bity zaciętych przycisków

  // ---------- log (port serwisowy) ----------
  void log(const char* f, ...) __attribute__((format(printf, 2, 3))) {
    char b[256];
    va_list v;
    va_start(v, f);
    int n = vsnprintf(b, sizeof b, f, v);
    va_end(v);
    if (n < (int)sizeof b) {
      a.port.wiersz(b);
      return;
    }
    std::string d((size_t)n + 1, '\0');
    va_start(v, f);
    vsnprintf(&d[0], d.size(), f, v);
    va_end(v);
    d.resize((size_t)n);
    a.port.wiersz(d);
  }

  static const char* nazwaPrzyczyny(PrzyczynaStartu p) {
    switch (p) {
      case PrzyczynaStartu::ZASILANIE: return "zasilanie";
      case PrzyczynaStartu::ZEWNETRZNY: return "reset";
      case PrzyczynaStartu::PROGRAMOWY: return "restart programowy";
      case PrzyczynaStartu::PANIKA: return "błąd programu";
      case PrzyczynaStartu::INT_WDT: return "strażnik przerwań";
      case PrzyczynaStartu::TASK_WDT: return "strażnik zadania";
      case PrzyczynaStartu::WDT: return "strażnik";
      case PrzyczynaStartu::USPIENIE: return "wybudzenie z uśpienia";
      case PrzyczynaStartu::SPADEK: return "spadek napięcia";
      case PrzyczynaStartu::NIEZNANA: return "nieznana";
      case PrzyczynaStartu::INNA: break;
    }
    return "inna";
  }

  void odnow() { a.straznik.odnow(); }
  uint32_t ms() { return a.zegar.ms(); }
  void czekaj(uint32_t m) { a.zegar.czekaj(m); }

  // ---------- czas ----------
  uint32_t teraz() { return a.zegar.teraz(); }
  bool zegar() { return zegarZnany(teraz()); }
  std::string dzisiaj() {
    std::tm t{};
    if (!zegar() || !a.zegar.lokalny(teraz(), t)) return "";
    char b[11];
    strftime(b, sizeof b, "%Y-%m-%d", &t);
    return b;
  }
  int minutaDoby() {
    std::tm t{};
    if (!zegar() || !a.zegar.lokalny(teraz(), t)) return 12 * 60;
    return t.tm_hour * 60 + t.tm_min;
  }
  std::string godzina(uint32_t ep) {
    std::tm t{};
    if (!ep || !a.zegar.lokalny(ep, t)) return "";
    char b[8];
    snprintf(b, sizeof b, "%02d:%02d", t.tm_hour, t.tm_min);
    return b;
  }
  std::string dataZ(uint32_t ep) {
    std::string d;
    int m;
    dataIMinuta(ep, d, m);
    return d;
  }
  void dataIMinuta(uint32_t ep, std::string& data, int& minuta) {
    data.clear();
    minuta = -1;
    std::tm t{};
    if (!ep || !a.zegar.lokalny(ep, t)) return;
    char b[11];
    strftime(b, sizeof b, "%Y-%m-%d", &t);
    data = b;
    minuta = t.tm_hour * 60 + t.tm_min;
  }

  Ekran ekran() { return ekranZRtc(r.ekran); }
  void ustawEkran(Ekran e) {
    if (e != ekran()) r.klatkaOd = 0;  // inny ekran: następne rysowanie zawsze odświeża e-ink
    r.ekran = (uint8_t)e;
  }
  Plotno& plotno() { return a.wyswietlacz.plotno(); }

  // ---------- pliki (przez magazyn, src/magazyn.h) ----------
  bool fsOk() { return a.pliki.zamontowane(); }
  // R-AW-16 (linia uwagi): pamięć plików się nie montuje (T-WYM-34) albo ma starą tablicę partycji, więc nic się nie
  // zapisuje do restartu (R7 karty „pracuje jak T-WYM-34”).
  bool awariaPamieci() { return !fsOk() || staryUklad; }

  // Wiersze pliku prototypu (wersja 0) przez adapter, jak w prototypie: przy nieudanej migracji (T-WYM-56) i dla
  // /import.csv (wejście jednorazowe, bez zmian).
  std::vector<std::string> czytajWiersze(const char* nazwa) {
    std::vector<std::string> w;
    if (fsOk()) a.pliki.czytajWiersze(nazwa, w);
    return w;
  }

  // Liczba pól wersji 1 formatu: tyle rozumieją parsery prototypu (wiersze z nowszej wersji cięte do nich).
  size_t polV1(const char* nazwa) {
    const FormatPliku* f = formatPliku(rej, nazwa);
    return f && !f->pola.empty() ? f->pola[0].size() : 0;
  }
  // Wiersze danych wszystkich plików formatu (pliki roczne od roku `odRoku`, "" = wszystkie), pola wersji 1 (noce od
  // pola wersji 2, POLA_NOCY_V2). Zapamiętuje
  // skrót wierszy każdej ścieżki (zapis tylko przy zmianie) i wersję z nagłówka (ekran testu).
  // `sciezkiWierszy` (może być nullptr): ścieżka pliku każdego zwróconego wiersza.
  std::vector<std::string> wczytaj(const char* nazwa, const std::string& odRoku = "",
                                   std::vector<std::string>* sciezkiWierszy = nullptr) {
    std::vector<std::string> w;
    if (!fsOk()) return w;
    // noce w polach wersji 2 (parsujNocV2 rozumie wszystkie pola wersji 2), pozostałe formaty w polach wersji 1
    size_t ile = std::strcmp(nazwa, "noce") == 0 ? POLA_NOCY_V5 : polV1(nazwa);
    if (std::strcmp(nazwa, "sypialnia") == 0) {  // wersja 2 (seria CO2 nocy) rozumiana przez parsujSypialnie
      const FormatPliku* f = formatPliku(rej, nazwa);
      if (f && !f->pola.empty()) ile = f->pola.back().size();
    }
    for (auto& s : mag.sciezki(nazwa)) {
      std::string rok = mag.sciezkaRoku(nazwa, "0000") == s ? "" : s.substr(s.size() - 8, 4);
      if (!odRoku.empty() && !rok.empty() && rok < odRoku) continue;
      OdczytPliku o = mag.czytaj(nazwa, s);
      if (o.stan == StanPliku::BRAK || o.stan == StanPliku::USZKODZONY) continue;
      wersjePlikow.insert(o.wersja);
      std::vector<std::string> v;
      v.reserve(o.wiersze.size());
      for (auto& x : o.wiersze) v.push_back(pierwszePola(x, ile));
      skrotPliku[s] = skrotWierszy(v);
      w.insert(w.end(), v.begin(), v.end());
      if (sciezkiWierszy) sciezkiWierszy->insert(sciezkiWierszy->end(), v.size(), s);
    }
    return w;
  }
  // Zapis pliku przepisywanego (pomija format czytany z pliku prototypu, T-WYM-56). `tylkoPrzyZmianie`: pliki roczne
  // (noce, sypialnia) bez zapisu, gdy wiersze tej ścieżki są takie jak przeczytane albo ostatnio zapisane, więc
  // przepisywany jest tylko rok, który się zmienił; pozostałe pliki jak w prototypie przy każdym wywołaniu.
  // Wynik: true, gdy na nośniku są te wiersze (zapisane albo bez zmian od odczytu);
  // false, gdy nie zostały zapisane (brak systemu plików, format tylko do odczytu, pełna pamięć, błąd zapisu).
  bool zapiszFormat(const char* nazwa, const std::string& sciezka, const std::vector<std::string>& w,
                    bool tylkoPrzyZmianie = false) {
    if (!fsOk() || bezZapisu.count(nazwa)) return false;
    uint32_t sk = skrotWierszy(w);
    auto i = skrotPliku.find(sciezka);
    if (tylkoPrzyZmianie && i != skrotPliku.end() && i->second == sk) return true;
    if (!mag.zapisz(nazwa, sciezka, w)) return false;
    skrotPliku[sciezka] = sk;
    return true;
  }
  void dopiszFormat(const char* nazwa, const std::string& wiersz) {
    if (fsOk() && !bezZapisu.count(nazwa)) mag.dopisz(nazwa, mag.sciezkaRoku(nazwa, ""), wiersz);
  }
  template <typename T, typename F>
  std::vector<std::string> wierszeZ(const std::vector<T>& v, F f) {
    std::vector<std::string> w;
    w.reserve(v.size());
    for (auto& e : v) w.push_back(f(e));
    return w;
  }

  void sortuj() {
    std::sort(hist.begin(), hist.end(), [](const Noc& x, const Noc& y) { return x.data < y.data; });
  }
  // Noce w plikach rocznych `/dane/noce/RRRR.csv` (rok daty poranka), z polem `import` (R1 karty).
  // false: zapis któregokolwiek pliku roku się nie udał; pozostałe lata są zapisane.
  bool zapiszHistorie() {
    std::map<std::string, std::vector<std::string>> lata;  // ścieżka pliku roku → wiersze
    for (auto& n : hist)
      if (n.data.size() == 10) lata[mag.sciezkaRoku("noce", n.data.substr(0, 4))].push_back(wierszNocyV5(n));  // wersja 5 (M-14, M-15)
    // wiersz, którego parser nie czyta, wraca do swojego pliku dosłownie, na miejsce
    // według pierwszego pola (przed wierszami z tym samym albo późniejszym), więc przepisanie pliku roku go nie gubi
    for (auto& kv : nieczytelneNoce) {
      std::vector<std::string>& w = lata[kv.first];
      for (auto& b : kv.second) {
        std::string kb = pierwszePola(b, 1);
        size_t i = 0;
        while (i < w.size() && pierwszePola(w[i], 1) < kb) i++;
        w.insert(w.begin() + (long)i, b);
      }
    }
    bool ok = true;
    for (auto& kv : lata) ok = zapiszFormat("noce", kv.first, kv.second, true) && ok;
    return ok;
  }
  void zapiszCzynniki() { zapiszFormat("czynniki", mag.sciezkaRoku("czynniki", ""), wierszeZ(czynniki, zapiszWpis)); }
  // Sypialnia w plikach rocznych `data;temp;wilg;co2` (odczyt czujnika i CO2 w jednym wierszu).
  void zapiszSypialnieNoce() {
    std::map<std::string, std::vector<std::string>> lata;
    std::vector<NocSypialni> v = sypNoce;
    std::sort(v.begin(), v.end(), [](const NocSypialni& x, const NocSypialni& y) { return x.data < y.data; });
    for (auto& n : v) {
      if (n.data.size() != 10) continue;
      std::string co2 = std::isnan(n.co2) ? "" : std::to_string((long)std::lround(n.co2));
      lata[n.data.substr(0, 4)].push_back(zapiszSypialnie(n) + ";" + co2 + polaCo2V2(n));  // wersja 2
    }
    for (auto& kv : lata) zapiszFormat("sypialnia", mag.sciezkaRoku("sypialnia", kv.first), kv.second, true);
  }

  // Pomiar baterii: wiersz /log/wybudzenia.csv na każde wybudzenie, przy zasypianiu.
  void wczytajBaterie() {
    baterie.clear();
    std::vector<std::string> w =
        bezZapisu.count("wybudzenia") ? czytajWiersze(formatPliku(rej, "wybudzenia")->sciezkaV0.c_str())
                                      : wczytaj("wybudzenia");
    for (auto& l : w) {
      WpisBaterii x;
      if (parsujBaterie(l, x)) baterie.push_back(x);
    }
    baterieWczytane = true;
  }
  void zapiszWpisBaterii() {
    uint32_t terazMs = ms();
    long wifiMs = (long)wifiLacznieMs + (wifiOdMs ? (long)(terazMs - wifiOdMs) : 0);
    if (!std::isnan(bateria) && zegar()) {
      WpisBaterii w;
      w.ep = teraz() - terazMs / 1000;  // chwila wybudzenia
      w.mV = (int)std::lround(bateria * 1000);  // lroundf: ten sam wynik co lround((double)…) prototypu
      w.wifiMs = wifiMs;
      w.aktywnyMs = r.aktywnyPoprzedniMs;
      w.powod = powodBaterii;
      if (baterieWczytane) baterie.push_back(w);
      dopiszFormat("wybudzenia", zapiszBaterie(w));
      // przycinanie (przepisanie dziennika): sprawdzane raz na dobę, bo pierwszy wiersz danych nie jest już pierwszym
      // wierszem pliku (nagłówek); prototyp czytał sam pierwszy wiersz w każdym wybudzeniu
      int32_t doba = dobaPamieci();
      if (fsOk() && doba && doba != r.wybudzeniaDoba && !bezZapisu.count("wybudzenia")) {
        r.wybudzeniaDoba = doba;
        if (!baterieWczytane) wczytajBaterie();
        if (!baterie.empty() && wymagaPrzyciecia(baterie.front().ep, teraz()) && przytnijBaterie(baterie, teraz()))
          mag.zapisz("wybudzenia", mag.sciezkaRoku("wybudzenia", ""), wierszeZ(baterie, zapiszBaterie));
      }
    }
    r.aktywnyPoprzedniMs = (int32_t)ms();  // ten zapis też liczy się do aktywności
  }

  void dopiszDziennik(const WpisHumoru& w) {
    dziennik.push_back(w);
    dopiszFormat("humor", zapiszHumor(w));
  }

  // Wszystkie pliki z pamięci jeszcze raz (po migracji w miejscu, która odrzuciła wcześniejszy zapis, T-WYM-55).
  void zapiszWszystko() {
    sortuj();
    zapiszHistorie();
    zapiszCzynniki();
    zapiszSypialnieNoce();
    if (!bezZapisu.count("humor")) mag.zapisz("humor", mag.sciezkaRoku("humor", ""), wierszeZ(dziennik, zapiszHumor));
    if (shWczytany) {
      sh.pomZm = sh.probyZm = true;
      zapiszStanHist(sh);
    }
  }

  // Zrzut plików przez port serwisowy (U2, komenda `Z`). pliki prototypu jak dotąd
  // (przed przejściem i po powrocie do poprzedniej wersji), potem pliki magazynu pod nowymi ścieżkami, które są.
  void zrzutPlikow() {
    a.port.wiersz("PLIKI_START");
    for (const char* nazwa : {"/noce.csv", "/czynniki.csv", "/humor.csv", "/sypialnia.csv", "/sypialnia-co2.csv",
                              "/bateria.csv"}) {
      if (!fsOk() || !a.pliki.istnieje(nazwa)) continue;  // U10: bez pustych sekcji plików, których nie ma
      log("PLIK %s", nazwa);
      for (auto& l : czytajWiersze(nazwa)) a.port.wiersz(l);
      a.port.wiersz("PLIK_KONIEC");
    }
    std::vector<std::string> sciezkiZrzutu;
    std::set<std::string> katalogi;
    for (auto& f : rej) {
      for (auto& sc : mag.sciezki(f.nazwa)) sciezkiZrzutu.push_back(sc);
      std::string kat = f.sciezka.substr(0, f.sciezka.rfind('/'));
      if (!kat.empty()) katalogi.insert(kat);
    }
    // kopie `{plik}.uszkodzony` (src/magazyn.h Magazyn::czytaj) też w zrzucie, do ręcznego
    // odzyskania do czasu przywracania z kopii na karcie
    for (auto& kat : katalogi) {
      std::vector<std::string> n;
      if (!a.pliki.listuj(kat.c_str(), n)) continue;
      for (auto& x : n)
        if (x.find(".uszkodzony") != std::string::npos) sciezkiZrzutu.push_back(x);  // także `.uszkodzony.{n}` (U7)
    }
    for (auto& sc : sciezkiZrzutu) {
      std::string t;
      if (!a.pliki.czytaj(sc.c_str(), t)) continue;  // surowa treść: zrzut niczego nie przywraca ani nie usuwa
      log("PLIK %s", sc.c_str());
      size_t p = 0;
      while (p < t.size()) {
        odnow();
        size_t e = t.find('\n', p);
        if (e == std::string::npos) e = t.size();
        if (e > p) a.port.wiersz(t.substr(p, e - p));
        p = e + 1;
      }
      a.port.wiersz("PLIK_KONIEC");
    }
    zrzutKonfiguracji();
    a.port.wiersz("PLIKI_KONIEC");
  }
  // `/konfiguracja.txt` w zrzucie (ustawienia i dane wpisane na panelu: daty wymiany, model maski, formularz), żeby
  // kopia zapasowa ją miała, a narzędzie mogło ją odczytać, zmienić i wgrać komendą `W konfiguracja.txt`. Wiersz
  // `konfiguracja w zrzucie: {n} B, sha256 {skrót}` przed sekcją pliku pozwala narzędziu złożyć treść bajt w bajt
  // (puste wiersze, koniec pliku bez `\n`) i wykryć błąd przesyłu. W logu tylko rozmiar i skrót, treść tylko w sekcji
  // pliku. Brak pliku: brak sekcji (jak przy plikach prototypu).
  void zrzutKonfiguracji() {
    std::string t;
    if (!fsOk() || !a.pliki.istnieje("/konfiguracja.txt") || !a.pliki.czytaj("/konfiguracja.txt", t)) return;
    log("konfiguracja w zrzucie: %lu B, sha256 %s", (unsigned long)t.size(), sha256Hex(t).c_str());
    log("PLIK /konfiguracja.txt");
    size_t p = 0;
    while (p < t.size()) {  // każdy wiersz, także pusty; `\n` na końcu nie daje pustego wiersza
      odnow();
      size_t e = t.find('\n', p);
      if (e == std::string::npos) e = t.size();
      a.port.wiersz(t.substr(p, e - p));
      p = e + 1;
    }
    a.port.wiersz("PLIK_KONIEC");
  }

  // ---------- historia nocy ----------
  void importujJesliCzas() {
    if (!fsOk() || staryUklad || !a.pliki.istnieje("/import.csv")) return;
    std::string dzis = dzisiaj();
    if (dzis.empty()) {
      log("import historii: czekam na zegar");
      return;
    }
    importWczytane = 0;
    importPominiete = 0;
    int uzupelnione = 0;
    bool pierwsza = true;
    for (auto& l : czytajWiersze("/import.csv")) {
      Noc n;
      bool ok = parsujNoc(l, 'K', n);
      if (pierwsza && !ok && l.rfind("data_poranka", 0) == 0) {  // nagłówek
        pierwsza = false;
        continue;
      }
      pierwsza = false;
      if (!ok || n.data > dzis) {
        importPominiete++;
        continue;
      }
      n.zrodlo = 'K';
      const Noc* stara = nocZDnia(hist, n.data);
      uint32_t importPrzed = stara ? stara->import : 0;  // R1 karty noc z karty bez chwili importu
      WynikImportu wi = wpiszImport(hist, n);
      for (auto& x : hist)
        if (x.data == n.data && !x.import) x.import = importPrzed;
      switch (wi) {
        case WynikImportu::DODANA:
        case WynikImportu::ZASTAPIONA: importWczytane++; break;
        case WynikImportu::UZUPELNIONA: uzupelnione++; break;
        case WynikImportu::POMINIETA: importPominiete++; break;
      }
    }
    sortuj();
    // import oznaczony jako wczytany dopiero po zapisie wszystkich plików roku. Bez
    // tego /import.csv zostaje i następne wybudzenie wczytuje go od nowa; ponowny import tych samych wierszy nie
    // dubluje nocy (wpiszImport()), a lata już zapisane nie są przepisywane (skrót w zapiszFormat()).
    if (!zapiszHistorie()) {
      log("import historii: zapis plików nocy nieudany, /import.csv zostaje do ponowienia");
      importWczytane = -1;  // diagnostyka bez „import +n”: nic nie oznaczono jako wczytane
      return;
    }
    a.pliki.usun("/import.wczytany");
    a.pliki.zmienNazwe("/import.csv", "/import.wczytany");
    log("import historii: wczytane %d, uzupełnione %d, pominięte %d", importWczytane, uzupelnione, importPominiete);
  }

  // Format, którego plik prototypu nie przeszedł migracji (T-WYM-56): czytany parserem wersji 0, bez zapisu.
  void poMigracjiNaNowe() {
    for (auto& w : mag.migracje()) {
      if (w.ok) continue;
      for (auto& f : rej)
        if (!f.sciezkaV0.empty() && nazwaPliku(f.sciezkaV0) == w.plik) bezZapisu.insert(f.nazwa);
      bledyMigracji.push_back("migracja " + w.plik + ": błąd");
    }
  }
  // Po oznaczeniu wersji jako dobrej (albo przy wersji już dobrej): migracje w miejscu i NVS (T-WYM-43); zapis
  // odrzucony przed migracją (plik w starszej wersji) powtórzony z pamięci.
  void poOznaczeniu() {
    if (fsOk()) {
      mag.migrujWMiejscu();
      for (auto& w : mag.migracje())
        if (!w.ok) bledyMigracji.push_back("migracja " + w.plik + ": błąd");
      if (mag.odrzuconoZapisStarszego()) zapiszWszystko();
    }
    // T-WYM-50: klucz `wersja` w przestrzeniach NVS tego wydania (przestrzeń z nowszej wersji bez zmian, T-WYM-54)
    if (std::find(nvsNowsze.begin(), nvsNowsze.end(), "cpap") == nvsNowsze.end())
      a.nvs.ustawLiczbeU("cpap", NVS_KLUCZ_WERSJA, WERSJA_NVS_CPAP);
    if (std::find(nvsNowsze.begin(), nvsNowsze.end(), "noc") == nvsNowsze.end())
      a.nvs.ustawLiczbeU("noc", NVS_KLUCZ_WERSJA, WERSJA_NVS_NOC);
  }
  // T-WYM-54 dla NVS: przestrzeń z kluczem `wersja` wyższym niż znana firmware tylko do odczytu do końca pracy.
  void sprawdzWersjeNvs() {
    struct P {
      const char* p;
      uint32_t znana;
    };
    for (P x : {P{"cpap", WERSJA_NVS_CPAP}, P{"noc", WERSJA_NVS_NOC}}) {
      uint32_t v = a.nvs.liczbaU(x.p, NVS_KLUCZ_WERSJA, 0);
      if (v <= x.znana) continue;
      nvsZm.tylkoDoOdczytu(x.p);
      nvsNowsze.push_back(x.p);
      log("NVS %s: dane z nowszej wersji (%lu, znana %lu), tylko do odczytu", x.p, (unsigned long)v,
          (unsigned long)x.znana);
    }
  }

  // Układ partycji i pierwsze formatowanie przy starcie (T-WYM-84): decyzja w rdzeniu
  // (src/magazyn.h decyzjaPartycji()), adapter tylko czyta tablicę, bloki partycji i formatuje.
  void sprawdzPartycje(bool zamontowana) {
    OpisPartycji op = a.pliki.partycja();
    StanPartycji s;
    s.zamontowana = zamontowana;
    s.ukladZgodny = op.uklad == ukladWkompilowany();
    if (s.ukladZgodny && zamontowana) return;
    if (s.ukladZgodny) {
      s.znacznikNvs = a.nvs.tekst("cpap", NVS_KLUCZ_FS_INIT, "");
      s.znacznikBiezacy = znacznikPartycji(op);
      s.ukladNvs = a.nvs.liczbaU("cpap", NVS_KLUCZ_UKLAD, 0);
      s.blokiCzytelne = a.pliki.czytajPartycje(2 * BLOK_PARTYCJI, s.bloki);
    }
    DecyzjaPartycji d = decyzjaPartycji(s);
    if (d == DecyzjaPartycji::STARY_UKLAD) {
      staryUklad = true;
      for (auto& f : rej)
        if (!f.sciezkaV0.empty()) bezZapisu.insert(f.nazwa);  // pliki prototypu tylko do odczytu
      log("układ partycji: stary (panel %08lx, firmware %08lx), wgraj kablem (8.9); bez formatowania, migracji i "
          "zapisu plików", (unsigned long)op.uklad, (unsigned long)ukladWkompilowany());
      return;
    }
    if (d != DecyzjaPartycji::FORMATUJ) {
      log("pamięć plików: nie zamontowana, bez formatowania (%s; T-WYM-34)", opisDecyzjiPartycji(d));
      return;
    }
    log("pamięć plików: pierwsze formatowanie (partycja nowa, 0x%lx, %lu B; T-WYM-84)", (unsigned long)op.przesuniecie,
        (unsigned long)op.rozmiar);
    a.straznik.zapisz(STRAZNIK_FORMATOWANIA_S, true);  // U13: formatowanie bez odnowienia strażnika
    bool sformatowana = a.pliki.formatuj();
    UstawieniaStraznika u = ustawieniaStraznika();
    a.straznik.zapisz(u.limitS, u.panika);
    if (!sformatowana) {
      log("pierwsze formatowanie: błąd, praca bez plików (T-WYM-34)");
      return;
    }
    a.nvs.ustawTekst("cpap", NVS_KLUCZ_FS_INIT, s.znacznikBiezacy);
    a.nvs.ustawLiczbeU("cpap", NVS_KLUCZ_UKLAD, ukladWkompilowany());
    // U2 wariant A: nowa partycja = przywracanie od nowa (znacznik udanego przywrócenia dotyczył poprzedniej)
    a.nvs.usun("cpap", NVS_KLUCZ_PRZYWR);
    a.nvs.usun("cpap", NVS_KLUCZ_PRZYWR_ODRZ);
    a.nvs.usun("cpap", NVS_KLUCZ_PRZYWR_K);
    a.nvs.usun("cpap", NVS_KLUCZ_PRZYWR_PROBA);
    sformatowano = true;
  }

  // T-WYM-62: przywrócenie ze zrzutu w `/przywracanie/` (scalenie, zapis i porównanie liczby nocy w magazynie, wynik
  // w logu). U2 wariant A i: znacznik
  // `cpap`/`przywr` = `{data} {liczba nocy}` w NVS tylko po udanym przywróceniu, gdy okno zakończyła komenda `K` przy
  // komplecie plików (NVS `przywr_k`, kompletPlikow()) i żaden plik zrzutu nie czeka na ponowne wgranie; od tej chwili
  // okno się nie otwiera. Przywrócenie przed oknem przy starcie (plik z wcześniejszego okna) stawia znacznik tylko
  // wtedy, gdy `K` z kompletem już było (zanik zasilania między `K` a znacznikiem); okno zakończone czasem albo
  // przyciskiem nie stawia go nigdy.
  void przywroc() {
    przywrocenie = mag.przywrocZKatalogu(KATALOG_PRZYWRACANIA);
    if (!przywrocenie.ok) {
      if (przywrocenie.proba) log("przywracanie: bez znacznika, okno przywracania wróci przy następnym starcie zasilania");
      return;
    }
    wczytajOdrzucone();
    if (!odrzucone.empty()) {
      log("przywracanie: pliki odrzucone w oknie (%s), bez znacznika; okno przywracania wróci przy następnym starcie "
          "zasilania", listaOdrzuconych().c_str());
      return;
    }
    if (!a.nvs.flaga("cpap", NVS_KLUCZ_PRZYWR_K, false) || !kompletPlikow()) {
      log("przywracanie: okno nie zakończone komendą K przy komplecie plików, bez znacznika; okno przywracania wróci "
          "przy następnym starcie zasilania");
      return;
    }
    std::string z = (dzisiaj().empty() ? std::string("?") : dzisiaj()) + " " + std::to_string(przywrocenie.przywrocone);
    a.nvs.ustawTekst("cpap", NVS_KLUCZ_PRZYWR, z);
    log("przywracanie: zakończone, znacznik %s w NVS (okno przywracania już się nie otworzy)", z.c_str());
  }
  // U2 wariant A: przywracanie czeka (okno przy starcie zasilania), dopóki w NVS nie ma znacznika udanego przywrócenia,
  // a partycja była sformatowana przez firmware (znacznik `fs_init`, czyli po przejściu 8.9; niezależnie od nocy
  // pobranych potem z AirBridge) albo nie ma plików nocy.
  bool przywracanieCzeka() {
    if (!fsOk() || staryUklad) return false;
    // także znacznik ZNACZNIK_BEZ_PRZYWRACANIA (okno bez żadnej komendy `W`) kończy czekanie
    if (!a.nvs.tekst("cpap", NVS_KLUCZ_PRZYWR, "").empty()) return false;
    return !a.nvs.tekst("cpap", NVS_KLUCZ_FS_INIT, "").empty() || bezNocy();
  }
  void wczytajOdrzucone() {
    if (odrzuconeWczytane) return;
    odrzuconeWczytane = true;
    std::string t = a.nvs.tekst("cpap", NVS_KLUCZ_PRZYWR_ODRZ, "");
    size_t p = 0;
    while (p < t.size()) {
      size_t e = t.find(',', p);
      if (e == std::string::npos) e = t.size();
      if (e > p) odrzucone.insert(t.substr(p, e - p));
      p = e + 1;
    }
  }
  std::string listaOdrzuconych() const {
    std::string t;
    for (auto& n : odrzucone) t += (t.empty() ? "" : ",") + n;
    return t;
  }
  void zapiszOdrzucone() {
    if (odrzucone.empty()) a.nvs.usun("cpap", NVS_KLUCZ_PRZYWR_ODRZ);
    else a.nvs.ustawTekst("cpap", NVS_KLUCZ_PRZYWR_ODRZ, listaOdrzuconych());
  }
  // komplet plików zrzutu = żaden plik zrzutu nie czeka na ponowne wgranie (lista odrzuconych
  // pusta) i plik nocy (`noce.csv`, bez niego przywrócenie nie ma czego liczyć, T-WYM-87 weryfikacja) leży
  // w `/przywracanie/`, czyli był potwierdzony `OK` w tym albo we wcześniejszym oknie (komenda `W` zapisuje tam plik
  // tylko przy `OK`). Pozostałe pliki zrzutu są opcjonalne (prototyp mógł ich nie mieć); ich wysłanie przed `K`
  // pilnuje narzędzie (`K` tylko, gdy każdy wysłany plik ma `OK`, wgraj_pliki.py wgraj()).
  bool kompletPlikow() {
    wczytajOdrzucone();
    return odrzucone.empty() && a.pliki.istnieje((std::string(KATALOG_PRZYWRACANIA) + "/noce.csv").c_str());
  }
  // okno przywracania tylko po prawdziwym starcie zasilania.
  // ESP_RST_POWERON: włączenie, wymiana baterii, a na ESP32-S3 także reset pinem EN (CHIP_PU) przez DTR/RTS mostka
  // CH340 (wgraj_pliki.py --reset, upload kablem), który rdzeń zgłasza jako POWERON. ESP_RST_EXT (reset pinem
  // zewnętrznym, gdy układ go zgłasza) też: to celowe działanie użytkownika przy panelu, tak samo jak odłączenie
  // zasilania. Nie: ESP_RST_SW (restart po aktualizacji przez Wi-Fi), błąd programu, strażniki, spadek napięcia,
  // wybudzenie z uśpienia.
  // Znacznik `przywr` = „bez przywracania”: okno zakończone bez żadnej komendy `W`.
  bool bezPrzywracania() {
    return a.nvs.tekst("cpap", NVS_KLUCZ_PRZYWR, "").compare(0, strlen(ZNACZNIK_BEZ_PRZYWRACANIA),
                                                            ZNACZNIK_BEZ_PRZYWRACANIA) == 0;
  }
  bool startZasilania() const {
    return przyczynaStartu == PrzyczynaStartu::ZASILANIE || przyczynaStartu == PrzyczynaStartu::ZEWNETRZNY;
  }
  // Partycja bez plików nocy (T-WYM-62, T-WYM-87): bez plików rocznych, bez `/noce.csv` i bez czekającego importu
  // z karty (`/import.csv` to też noce, T-DANE-33).
  bool bezNocy() { return mag.bezPlikowNocy() && !a.pliki.istnieje("/import.csv"); }

  void wczytajPliki(PowodWybudzenia w) {
    bool zamontowane = a.pliki.zamontuj();  // bez formatowania: nieudane montowanie nie kasuje historii
    sprawdzPartycje(zamontowane);           // R7 i pierwsze formatowanie (T-WYM-84)
    zamontowane = fsOk();
    if (zamontowane && !staryUklad) {
      // T-WYM-62: przy starcie z partycją bez plików nocy najpierw zrzut wgrany wcześniej komendą `W` (przed
      // pobraniem i przed oknem przywracania); R-DANE-37: partycja z nocami nie czyta `/przywracanie/`
      // U2 wariant A: dopóki przywracanie czeka (przywracanieCzeka()); przy plikach z wcześniejszego okna
      if (w == PowodWybudzenia::START && przywracanieCzeka()) przywroc();
      // T-WYM-43, R9 karty migracja na nowe ścieżki przed oznaczeniem wersji (pliki prototypu nietknięte);
      // migracje w miejscu i NVS dopiero po oznaczeniu (poOznaczeniu(); przy wersji już dobrej od razu)
      // CRC plików prototypu tylko przy starcie (zmienia je wyłącznie poprzednia wersja firmware)
      migracjeOk = mag.migrujNaNoweSciezki(w == PowodWybudzenia::START);
      poMigracjiNaNowe();
    }
    // R7: przy starej tablicy ani migracji w miejscu, ani klucza `wersja` w NVS (panel wraca do starego firmware)
    if (!staryUklad && !samosprawdzenieWymagane(a.uruchomienie.stanWersji())) poOznaczeniu();
    if (!zamontowane) {
      log("LittleFS: nie zamontowany");
      return;
    }
    wczytajDane();
  }

  // Historia, czynniki, dziennik i sypialnia z plików do pamięci.
  void wczytajDane() {
    std::string dzis = dzisiaj(), granica = dzis.empty() ? "" : dodajDni(dzis, -120);
    std::string odRoku = granica.empty() ? "" : granica.substr(0, 4);
    hist.clear();
    if (bezZapisu.count("noce")) {
      for (auto& l : czytajWiersze("/noce.csv")) {
        Noc n;
        if (parsujNoc(l, 'A', n)) hist.push_back(n);
      }
    } else {
      nieczytelneNoce.clear();
      std::vector<std::string> sc;
      std::vector<std::string> wn = wczytaj("noce", "", &sc);
      for (size_t i = 0; i < wn.size(); i++) {
        Noc n;
        if (parsujNocV5(wn[i], n)) {
          hist.push_back(n);  // wiersze wersji 1…5
        } else {  // zachowany dosłownie do następnego przepisania pliku
          nieczytelneNoce[sc[i]].push_back(wn[i]);
          log("noce: wiersz nieczytelny w %s zachowany bez zmian (%s)", nazwaPliku(sc[i]).c_str(),
              pierwszePola(wn[i], 1).substr(0, 10).c_str());
        }
      }
    }
    sortuj();
    importujJesliCzas();
    czynniki.clear();
    for (auto& l : bezZapisu.count("czynniki") ? czytajWiersze("/czynniki.csv") : wczytaj("czynniki")) {
      WpisCzynnikow w;
      if (parsujWpis(l, w)) czynniki.push_back(w);
    }
    odpowiedzi.clear();
    for (auto& l : wczytaj("odpowiedzi")) {
      sprawy::Odpowiedz o;
      if (sprawy::parsujOdpowiedz(l, o)) odpowiedzi.push_back(o);
    }
    b5.dzis.clear();  // nowe dane: wyniki silnika od nowa
    dziennik.clear();
    size_t wszystkie = 0;
    for (auto& l : bezZapisu.count("humor") ? czytajWiersze("/humor.csv") : wczytaj("humor")) {
      WpisHumoru w;
      wszystkie++;
      if (parsujHumor(l, w) && w.data >= granica) dziennik.push_back(w);
    }
    if (wszystkie != dziennik.size() && !bezZapisu.count("humor"))  // dziennik: 120 dni, przycięcie przepisaniem
      mag.zapisz("humor", mag.sciezkaRoku("humor", ""), wierszeZ(dziennik, zapiszHumor));
    sypNoce.clear();
    if (bezZapisu.count("sypialnia")) {  // pliki prototypu: /sypialnia.csv i /sypialnia-co2.csv
      for (auto& l : czytajWiersze("/sypialnia.csv")) {
        NocSypialni n;
        if (parsujSypialnie(l, n) && n.data >= granica) sypNoce.push_back(n);
      }
      for (auto& l : czytajWiersze("/sypialnia-co2.csv")) {
        std::string d;
        float c;
        if (!parsujCo2Nocy(l, d, c) || d < granica) continue;
        NocSypialni* n = nullptr;
        for (auto& x : sypNoce)
          if (x.data == d) n = &x;
        if (!n) {
          sypNoce.push_back(NocSypialni{d, NAN, NAN});
          n = &sypNoce.back();
        }
        n->co2 = c;
      }
    } else {  // pliki roczne od roku granicy 120 dni (cały rok w pamięci: zapis roku bez utraty starszych wierszy)
      for (auto& l : wczytaj("sypialnia", odRoku)) {
        NocSypialni n;
        if (parsujSypialnie(l, n)) sypNoce.push_back(n);
      }
    }
    log("historia: %d nocy, czynniki: %d, dziennik ekranów: %d", (int)hist.size(), (int)czynniki.size(),
        (int)dziennik.size());
  }
  // stany odchylenia (4.6) i norma z wyłączeniami 4.4 (src/stany.h)
  const Stany& stany(const std::string& dzis) { return stanyZPamieci(histNormyB16(dzis), dzis, wykluczoneB16(dzis)); }  // z nocami okresów infekcji
  Norma norma(const std::string& dzis) { return stany(dzis).norma; }

  // M-22: odczyt czujnika do bufora nocy; od 06:00 mediana nocy do /sypialnia.csv
  void sypialniaTeraz() {
    std::string dzis = dzisiaj();
    if (dzis.empty()) return;
    NocSypialni n;
    if (r.buforSyp.n > 0 && r.buforSyp.data[0]) {  // minimum wilgotności nocy dla silnika (sygnał wilgotnosc_min_nocy)
      int mn = 100000;
      for (int i = 0; i < r.buforSyp.n && i < BUFOR_SYPIALNI_ODCZYTOW; i++) mn = std::min(mn, (int)r.buforSyp.w10[i]);
      if (mn >= 0 && mn < 100000) {
        a.nvs.ustawTekst("cpap", "wmin_d", r.buforSyp.data);
        a.nvs.ustawLiczbeU("cpap", "wmin", (uint32_t)(mn + 1));
      }
    }
    if (zamknijNoc(r.buforSyp, dzis, minutaDoby(), n) && !sypialniaZDnia(sypNoce, n.data)) {
      sypNoce.push_back(n);
      zapiszSypialnieNoce();
    }
    dodajOdczyt(r.buforSyp, dzis, minutaDoby(), temp, wilg);
  }

  // ---------- czynniki: poranek ----------
  void poranekCzynniki() {
    std::string dzis = dzisiaj();
    if (poranekCzynnikow(czynniki, hist, dzis, minutaDoby(), norma(dzis), ziarno, okolicznosciRanka(dzis)))
      zapiszCzynniki();
  }
  // R-CZY-01 (wzorzec A), R-CZY-07 (S-K-17), R-CZY-15 (S-K-30), R-CZY-17 (prośba na ekranie) z silnika reguł
  OkolicznosciRanka okolicznosciRanka(const std::string& dzis) {
    OkolicznosciRanka ok;
    // R-CZY-15: S-K-30 nie jest wynikiem silnika reguł, tylko kandydatem katalogu S-K (sk::rano(), jak w pytanieB16());
    // bez tego sprawdzenia linia zaproszenia stała na S-K-30, gdzie ↻ znaczy „pomiń”, a ↻ 2 s nic nie robi
    ok.sk30 = sk30Kandydatem(dzis);
    // R-CZY-07, R-PIE-10: ekran S-K w tonie „problem” (R5b…R5d, np. S-K-17, S-K-10) jako ekran dnia to prośba, a ↻ 2 s
    // na nim znaczy „wiem” (mapa 5.6.2), więc bez linii zaproszenia; S-K-31 wyjęty, bo 5.11.3 dopuszcza na nim linię
    ok.prosbaNaEkranie = prosbaSkRano(dzis);
    if (!silnikDzis(dzis)) return ok;
    // sprawa silnika jako ekran dnia to prośba, chyba że ekran zajmie S-K-31 (na czele R5c przed L-09 i zdarzeniami
    // R5c, jak w rysujB16() etap 0), na którym zaproszenie jest dozwolone (7.4)
    const sprawy::SprawaL* l = sprawy::sprawaEkranuDnia(b5.sprawy, dzis);
    if (l && !zuzycieMaskiZamiastSprawy(dzis, *l)) ok.prosbaNaEkranie = true;
    // R-PIE-10, 7.4: pytanie `Wymieniona maska?` (R-MAS-12) jest prośbą; jeśli stoi dziś albo już na nie odpowiedziano
    // (także pominięciem), linia zaproszenia tego dnia nie wraca (R-CZY-25); pytanie dalej z K1 (R-CZY-08)
    bool poduszka = (b5.pytanie.jest && b5.pytanie.pytanie == sprawy::Pytanie::PODUSZKA && b5.pytanie.data == dzis) ||
                    sprawy::jestOdpowiedz(odpowiedzi, dzis, sprawy::Pytanie::PODUSZKA);
    ok.prosbaNaEkranie = ok.prosbaNaEkranie || poduszka;
    ok.wzorzecA = kolejka::niewyciszonyA(b5.sprawy, dzis);
    for (auto& w : b5.wyniki) {
      if (!w.oceniona || !w.spelniona) continue;
      if (w.id == "S-K-30") ok.sk30 = true;
      if (w.id == "S-K-17") ok.sk17 = true;
    }
    return ok;
  }
  // Czy S-K-30 jest dziś kandydatem slotu porannego (R5a, R-CZY-15). Przyjmuje datę poranka; zwraca false bez zegara.
  bool sk30Kandydatem(const std::string& dzis) {
    if (dzis.empty() || !zegar()) return false;
    DaneDnia d0;
    for (auto& t : sk::rano(wejscieB16(dzis, d0)))
      if (t.id == "S-K-30" && !t.doLogu) return true;
    return false;
  }
  // Czy rano zamiast sprawy silnika `l` (L-09 albo zdarzenie E) stanie S-K-31: pierwszy kandydat sk::rano() poza
  // wpisami do logu i S-K-30 to S-K-31, a grupa sprawy nie jest wyższa niż R5c (rysujB16() etap 0). Potrzebne, bo
  // sprawa silnika liczy się jako prośba na ekranie, a gdy ekran zajmie S-K-31, zaproszenie jest na nim dozwolone
  // (7.4, mapa 5.6.2) i nie może zgasnąć przez sprawę, której tego ranka nie widać.
  bool zuzycieMaskiZamiastSprawy(const std::string& dzis, const sprawy::SprawaL& l) {
    if (dzis.empty() || !zegar() || (l.id != "L-09" && !l.jestE())) return false;
    int rl = sk::ranga(l.id == "L-09" ? std::string("R5c") : l.grupa);
    DaneDnia d0;
    for (auto& t : sk::rano(wejscieB16(dzis, d0))) {
      if (t.doLogu || t.id == "S-K-30") continue;
      return t.id == "S-K-31" && sk::ranga(t.grupa) <= rl;
    }
    return false;
  }
  // Czy rano ekranem dnia stanie ekran S-K w tonie „problem” z grup R5b…R5d (prośba, 7.4). Patrzy tylko na kandydata,
  // który się pokaże (sk::problemNaEkranieRano(), ta sama kolejność co w rysujB16()), bo niższy kandydat, np. S-K-10
  // pod S-K-31, gasiłby zaproszenie na ekranie, który je dopuszcza. Przyjmuje datę poranka; false bez zegara.
  bool prosbaSkRano(const std::string& dzis) {
    if (dzis.empty() || !zegar()) return false;
    DaneDnia d0;
    return sk::problemNaEkranieRano(sk::rano(wejscieB16(dzis, d0)));
  }
  bool zaproszenieTeraz() {
    std::string dzis = dzisiaj();
    return !dzis.empty() && oknoDnia(minutaDoby()) && zaproszenieAktywne(wpisZDnia(czynniki, dzis), dzis, minutaDoby());
  }

  // zBaterii = false: bez E-05 krytycznego (tylko przyczyny po stronie AirBridge, np. wejście ekranów S-K)
  WejscieDnia wejscieDnia(bool zBaterii = true) {
    WejscieDnia w;
    w.dzis = dzisiaj();
    w.minuta = minutaDoby();
    w.hist = &hist;
    w.odczytOk = odczytUdany;
    if (r.ab.ma) dataIMinuta(r.ab.pobrano, w.abData, w.abMinuta);
    w.kluczBraku = std::to_string(r.ab.pobrano);
    w.kartaBraku = kartaBraku(httpKod);  // U6
    w.e03 = httpKod == KOD_E03;
    w.wBudowie = r.ab.wBudowie;
    w.terapia = terapiaTrwa(odczytUdany, r.ab.rop);  // `rop` ze starego odczytu nie blokuje S10
    w.maNorme = !w.dzis.empty() && norma(w.dzis).m01.jest();
    w.sypialnia = !w.dzis.empty() && sypialniaZDnia(sypNoce, w.dzis) &&
                  liczNormeSypialni(sypNoce, w.dzis, nocePozaDomem(czynniki)).t.jest();
    if (zBaterii) w.e05Krytyczne = e05KrytyczneTeraz(w.dzis);  // jedno źródło dla wyboru, nocy i przycisków
    return w;
  }
  // E-05 krytyczne jako przyczyna S5 (R1, 6.4), liczone w jednym miejscu: wybór
  // ekranu dnia, decyzja o S9 (S5 zostaje do 05:00, N0, R-PIE-02), nawigacja (↻ = nowy pomiar) i stopka widzą to samo.
  // Wynik silnika reguł z tego wybudzenia (M-25 z napięcia albo ostatni procent z RTC), także w slocie nocnym;
  // nie w dniu pełnego ekranu wzorca A (R-PIE-07: wtedy linia `Naładuj ekran: bateria {x}%.`). W trakcie silnikDzis()
  // (bramki pytają o S5) false: wynik jeszcze się liczy.
  bool e05KrytyczneTeraz(const std::string& dzis) {
    if (!dataOk(dzis) || wSilniku) return false;
    if (b5.dzis != dzis && !silnikDzis(dzis)) return false;
    return e05KrytyczneB5() && !dzienPelnegoA(sprawaB5(b5.kluczEkranu));
  }
  // S5 na ekranie dnia ma klucz `E-05` (bateria, nie AirBridge)
  bool s5BateriiTeraz() {
    WejscieDnia w = wejscieDnia();
    return brakDanych(w) && !brakDanychAirBridge(w);
  }
  bool s5Teraz() { return brakDanych(wejscieDnia()); }
  bool s10Teraz() { return synchronizacja(wejscieDnia()); }

  // ---------- rysowanie (…) ----------
  // ---------- silnik reguł → sprawy, kolejka, ekran dnia ----------
  // Sygnały komponentów dla zdarzeń E (reguly_sygnaly.h). Sygnał bez źródła danych zostaje poza
  // mapą („nie oceniam”, nigdy „w porządku”): M-25 tylko z odczytem napięcia w tym wybudzeniu (E-05, 6.4).
  reguly::Sygnaly sygnalyB5(const std::string& dzis) {
    reguly::Sygnaly s;
    if (procentBat >= 0) s.odczyty["M-25"] = procentBat;  // bateria ekranu w % (krzywa [PRÓG])
    // E-10 dla nocy bez rekordu (5.1.4): tylko stan „bez terapii” (AirBridge odpowiada, po 11:00 dnia nie ma
    // na liście, status bez stanu z R-DANE-35); przy E-01 i nocy w budowie nie (R-DANE-20, R-DANE-35)
    s.stan["noc_bez_rekordu"] = !nocZDnia(hist, dzis) && stan_nocy::bezTerapii(fazaNocy(), dzis);
    // E-04: noc częściowa staje się rekordem „częściowa” od 11:00 (R-DANE-15)
    s.stan["po_11"] = minutaDoby() >= 11 * 60;
    // E-07: czujnik w obudowie panelu nie dał odczytu w tym wybudzeniu (R-AW-06)
    s.stan["czujnik_sypialni_milczy"] = std::isnan(temp) && std::isnan(wilg);
    // E-08: ostrzeżenie `co2_milczy` z lokalnego źródła otoczenia; bez źródła (ADR-0019) nie ma czego zgłaszać
    bool co2 = co2B17() && zegar() && co2MilczyTeraz(r.oto, teraz());  // tylko z ekranami CO2
    s.stan["co2_milczy"] = co2;
    if (co2) {
      std::string dOd;
      int mOd;
      dataIMinuta(r.oto.co2MilczyOd, dOd, mOd);
      s.zmienne["czas"] = kiedyOdczyt(dOd, mOd, dzis);
    }
    // wilgotność najniższa z odczytów nocy (bufor sypialni, minimum zapamiętane przy zamknięciu nocy o 06:00)
    if (a.nvs.tekst("cpap", "wmin_d", "") == dzis) {
      uint32_t w = a.nvs.liczbaU("cpap", "wmin", 0);
      if (w > 0) s.odczyty["wilgotnosc_min_nocy"] = (w - 1) / 10.0;
    }
    return s;
  }
  std::vector<reguly::Wynik> ocenSilnikiem(const std::string& dzis) {
    reguly::Odpowiedzi odp = sprawy::odpowiedziSilnika(odpowiedzi);
    b5.sygnaly = sygnalyB5(dzis);
    if (sprawy::zrodloTestowe.sygnaly) sprawy::zrodloTestowe.sygnaly(b5.sygnaly);
    if (sprawy::zrodloTestowe.wyniki) {  // testy i symulator: wyniki zastępcze zamiast silnika reguł
      reguly::Kontekst kx;
      kx.historia = &hist;
      kx.dzis = dzis;
      kx.odpowiedzi = &odp;
      kx.pamiec = &b5.pamiec;
      b5.wykluczone = wykluczoneB16(dzis);  // R-INF-05
      kx.wykluczoneNoce = &b5.wykluczone;
      kx.zestaw = sprawy::zrodloTestowe.zestaw;
      kx.poczatekLeczenia = ustawieniaB9().poczatekLeczenia;  // R-SKI-08
      return sprawy::zrodloTestowe.wyniki(kx);
    }
#if CPAP_SILNIK_REGUL
    static bool rodzaje = false;  // rodzaje spoza rdzenia przed pierwszym zestawWbudowany() (rodzaje E; L niżej)
    if (!rodzaje) {
      reguly::zarejestrujRodzajeE();
      wzorce::zarejestrujRodzajeL();
      odkrycia::zarejestrujRodzajeO();
      rodzaje = true;
    }
    reguly::Kontekst kx;
    kx.historia = &hist;
    kx.dzis = dzis;
    kx.norma = &stany(dzis).norma;
    kx.stany = &stany(dzis);
    kx.trybBiezacy = trybHist();
    b5.wykluczone = wykluczoneB16(dzis);  // R-CZY-05: ten sam zbiór co liczNorme i Stany; R-INF-05
    kx.wykluczoneNoce = &b5.wykluczone;
    kx.odpowiedzi = &odp;
    kx.pamiec = &b5.pamiec;
    kx.zestaw = &reguly::zestawWbudowany();
    kx.poczatekLeczenia = ustawieniaB9().poczatekLeczenia;  // R-SKI-08: karencja L-01 tylko od znanej daty
    reguly::OpcjeOceny op;
    op.sygnaly = &b5.sygnaly;
    op.log = [this](const std::string& l) { log("%s", l.c_str()); };
    // wiersze odkryć poza dzienną oceną E/L/H (nie są sprawami pacjenta, src/odkrycia.h)
    return reguly::ocen(odkrycia::bezOdkryc(reguly::zestawWbudowany()), kx, op);
#else
    return {};
#endif
  }
  // Zapis plików (pamięć reguł, odpowiedzi): tylko przy zamontowanej pamięci w układzie wkompilowanym. Stary układ
  // partycji (R7 karty T-WYM-84: bez formatowania, migracji i zapisu) albo format tylko do odczytu: stan zostaje
  // w RAM tego wybudzenia, ocena działa dalej, zapis odroczony do pierwszego wybudzenia, w którym magazyn pisze.
  bool zapisB5() { return fsOk() && !staryUklad; }
  void zapiszOdpowiedzi() {
    if (!zapisB5()) return;
    zapiszFormat("odpowiedzi", mag.sciezkaRoku("odpowiedzi", ""), wierszeZ(odpowiedzi, sprawy::wierszOdpowiedzi));
  }
  // Pamięć reguł (src/reguly_pamiec.h): `aktywna_od`, „wiem”, „Omówione” przeżywają sen i restart.
  void zapiszPamiecRegul() {
    if (!zapisB5() || bezZapisu.count("reguly")) return;
    if (!reguly::zapiszPamiec(mag, b5.pamiec)) log("pamięć reguł: błąd zapisu");
  }
  // Raz na datę (i po nowych danych): wyniki, sprawy, ekran L dnia, kolejka linii, uspokojenie; log z kontekstem.
  bool silnikDzis(const std::string& dzis, const std::string& zaproszenie = "", const std::string& e08 = "",
                  const std::string& awaria = "") {
    if (dzis.empty()) return false;
    struct Straz {
      bool& f;
      bool przed;
      ~Straz() { f = przed; }
    } straz{wSilniku, wSilniku};
    wSilniku = true;
    if (b5.dzis != dzis) {
      if (!b5.pamiecWczytana && fsOk()) {
        b5.pamiec = reguly::czytajPamiec(mag);
        b5.pamiecWczytana = true;
      }
      b5.wyniki = ocenSilnikiem(dzis);
      b5.dzis = dzis;
      if (!b5.wyniki.empty()) {  // R-BUZ-04, rytm 5.12.2: data wejścia trwała (aktualizujPamiec)
        wzorce::Okolicznosci ow;
        bramkiB16(dzis, ow);  // bramki 7.3 (okres infekcji, element po terminie) przed pamięcią i stanami wzorców
        sk::zastosujSk10Kilka(b5.wyniki, sk::sk10Kilka(&hist, dzis, ow.okresInfekcji));  // 6.4 E-23
        reguly::Pamiec przed = b5.pamiec;
        reguly::aktualizujPamiec(b5.pamiec, b5.wyniki, dzis);
        // stany wzorców L (5.12.12) po pamięci reguł, przed zapisem; „wiem” i „Omówione” zapisuje
        const reguly::Zestaw* zw = sprawy::zrodloTestowe.zestaw;
#if CPAP_SILNIK_REGUL
        if (!zw) zw = &reguly::zestawWbudowany();
#endif
        ow.historia = &hist;
        wzorce::aktualizuj(b5.wyniki, b5.pamiec, dzis, zw, ow);
        bool zmiana = przed.size() != b5.pamiec.size();
        for (auto& kv : b5.pamiec) {
          auto i = przed.find(kv.first);
          zmiana = zmiana || i == przed.end() || i->second.aktywnaOd != kv.second.aktywnaOd ||
                   i->second.ostatnio != kv.second.ostatnio || i->second.licznik != kv.second.licznik ||
                   i->second.stanWzorca != kv.second.stanWzorca;
        }
        if (zmiana) zapiszPamiecRegul();
      }
      b5.termin = terminRanoB5(dzis);
      // bieżąca karta „Otwartych spraw” przeżywa sen (NVS `cpap`/`b5_karta`, jak kursor „Sprzęt”): wcześniej każde
      // wybudzenie zaczynało od karty 1, więc ▶ po przerwie dłuższej niż czuwanie dawało w kółko kartę 2
      b5.karta = std::max(1, (int)a.nvs.liczbaU("cpap", "b5_karta", 1));
      int spel = 0, nieN = 0;
      std::string nieOc;
      for (auto& w : b5.wyniki) {  // tylko reguły z poziomem (wiersze pomocnicze: log silnika `reguly: … nie oceniam`)
        if (w.poziom.empty()) continue;
        spel += w.oceniona && w.spelniona;
        if (!w.oceniona && w.stan != reguly::StanReguly::ZABLOKOWANA && ++nieN <= 12)
          nieOc += (nieOc.empty() ? "" : ", ") + sprawy::klucz(w) + " (" + w.powodNieoceny + ")";
      }
      if (nieN > 12) nieOc += ", … i " + std::to_string(nieN - 12) + " innych";
      if (!b5.wyniki.empty())
        log("silnik reguł %s: %u reguł, spełnione %d, nie oceniam: %s", dzis.c_str(), (unsigned)b5.wyniki.size(), spel,
            nieOc.empty() ? "brak" : nieOc.c_str());
    }
    if (b5.wyniki.empty()) return false;
    if (sprawy::zrodloTestowe.wyniki && sprawy::zrodloTestowe.zestaw) {
      b5.sprawy = sprawy::sprawyOtwarte(*sprawy::zrodloTestowe.zestaw, b5.wyniki, b5.pamiec, sprawy::zrodloTestowe.tekst);
    }
#if CPAP_SILNIK_REGUL
    else {
      const reguly::Zestaw& z = reguly::zestawWbudowany();
      reguly::Tryb tryb = trybHist();
      sprawy::FunkcjaTekstu f = [&z, tryb](const std::string& id, const reguly::Wynik& w) {
        return reguly::tekst(z, id, w, tryb);
      };
      b5.sprawy = sprawy::sprawyOtwarte(z, b5.wyniki, b5.pamiec, f);
    }
#endif
    if (ustawieniaB9().podpowiedz == 0)  // `podpowiedz=nie`: ekrany L bez ramki hipotezy (R-HIP-02)
      for (auto& sp : b5.sprawy) sp.hipoteza.clear();
    sprawy::stanyWzorcow(b5.sprawy, b5.wyniki, b5.pamiec, dzis, zestawB11());  // R-LEK-27
    const sprawy::SprawaL* e = sprawy::sprawaEkranuDnia(b5.sprawy, dzis);
    b5.kluczEkranu = e ? e->klucz : "";
    reguly::Odpowiedzi odp = sprawy::odpowiedziSilnika(odpowiedzi);
    kolejka::WejscieKolejki wk;
    wk.dzis = dzis;
    if (const reguly::Wynik* bk = e05KrytyczneB5())  // R-PIE-07: E-05 krytyczne ustępuje pełnemu ekranowi A
      if (dzienPelnegoA(e)) wk.bateriaKrytyczna = tekstB5("E-05.linia_krytyczna", *bk);
    wk.sprawy = &b5.sprawy;
    wk.odpowiedzi = &odp;
    wk.ekranDnia = e;
    wk.zaproszenie = zaproszenie;
    wk.e08 = e08;
    wk.awaria = awaria;
    // rytmy linii według pokazań (R-PIE-08), R-NOR-21, R-SKI-39, slot tygodniowy (R-SKI-12, R-SKI-33, R-SKI-41);
    // okres infekcji (R-INF-10): moduł INFEKCJA (src/infekcja.h)
    std::vector<infekcja::Okres> okB16 = okresyB16(dzis);
    const infekcja::Okres* okDzis = infekcja::okresDzis(okB16, dzis);
    if (okDzis) wk.okresInfekcjiOd = okDzis->od;
    wk.pamiec = &b5.pamiec;
    wk.minuta = minutaDoby();
    b5.metryki = sprawy::sprawyMetryk(hist, stany(dzis), dzis);
    wk.metryki = &b5.metryki;
    wk.odpowiedziPelne = &odpowiedzi;
    wk.nocyNieczytelne = stany(dzis).nocyNieczytelne;  // R-SKI-24
    // ADR-0039 pkt 5 (R-KON-68): linia „dobra zmiana” w grupie „norma”, raz na 7 dni od pokazania
    wk.dobraZmiana = sk::liniaDobrejZmiany(stany(dzis), trybHist() == reguly::Tryb::ASV ||
                                                        trybHist() == reguly::Tryb::ASVAUTO);
    b5.linieE = linieZdarzenB11();  // E-10, E-04 i inne linie zdarzeń w kolejce 7.5
    liniaZmianyUstawienB21(dzis);  // R-NOR-14 (grupa 4, przed R-MAS-11)
    liniaTrybuB21(dzis);           // R-DANE-29 (grupa 3)
    liniaNowegoModeluB16(dzis);  // R-MAS-11
    wk.linieE = &b5.linieE;
    b5.linie = kolejka::kolejkaLinii(wk);
    b5.stany = stany(dzis);
    kolejka::zastosujUspokojenie(b5.stany, kolejka::ocenUspokojenie(b5.wyniki, b5.linie));
    kolejka::WejsciePytan wp;
    wp.dzis = dzis;
    wp.minuta = minutaDoby();
    wp.sprawy = &b5.sprawy;
    wp.odpowiedzi = &odpowiedzi;
    wp.hist = &hist;
    wp.terminRano = b5.termin;
    int od = (int)a.nvs.liczbaU("cpap", "sen_od", 0) - 1;  // pierwsze pokazanie pytania o senność (minuta + 1)
    wp.pokazanoOd = a.nvs.tekst("cpap", "sen_d", "") == dzis ? od : -1;
    if (okDzis) {  // R-INF-07: pytanie „gorzej” w okresie infekcji
      std::vector<std::string> pg;
      for (auto& x : odpowiedzi)
        if (x.pytanie == sprawy::Pytanie::GORZEJ && x.data >= okDzis->od) pg.push_back(x.data);
      wp.gorzejInfekcja = infekcja::pytanieGorzej(*okDzis, hist, pg, dzis);
    }
    b5.pytanie = kolejka::pytanieDnia(wp);
    pytanieWyjazduB15(dzis, wp);
    pytanieB16(dzis, e);  // S-K-30 (R5a) i `Wymieniona poduszka?` (R8), gdy nie ma pytania R0
    return true;
  }
  // linie zdarzeń E z wyników, teksty z zestawu (atrapa testów albo wbudowany)
  std::vector<sprawy::LiniaZdarzenia> linieZdarzenB11() {
    if (sprawy::zrodloTestowe.wyniki && sprawy::zrodloTestowe.zestaw)
      return sprawy::linieZdarzen(*sprawy::zrodloTestowe.zestaw, b5.wyniki, sprawy::zrodloTestowe.tekst);
#if CPAP_SILNIK_REGUL
    const reguly::Zestaw& z = reguly::zestawWbudowany();
    reguly::Tryb tryb = trybHist();
    return sprawy::linieZdarzen(z, b5.wyniki, [&z, tryb](const std::string& id, const reguly::Wynik& w) {
      return reguly::tekst(z, id, w, tryb);
    });
#else
    return {};
#endif
  }
  // E-05 krytyczne (6.4, bateria ≤ 5%): wynik silnika z tego wybudzenia, oceniony i spełniony; nullptr = nie ma
  const reguly::Wynik* e05KrytyczneB5() {
    for (auto& w : b5.wyniki)
      if (w.id == "E-05" && w.wariant == "krytyczne" && w.oceniona && w.spelniona &&
          w.stan == reguly::StanReguly::AKTYWNA)
        return &w;
    return nullptr;
  }
  // R-PIE-07: ekranem dnia jest dziś pełny ekran wzorca L z pilnością A
  static bool dzienPelnegoA(const sprawy::SprawaL* e) { return e && e->jestL() && e->pilnosc == reguly::Pilnosc::A; }
  // Tekst z zestawu (atrapa testów albo wbudowany) dla wyniku; "" = brak
  std::string tekstB5(const std::string& id, const reguly::Wynik& w) {
    std::string t;
    if (sprawy::zrodloTestowe.wyniki && sprawy::zrodloTestowe.tekst) t = sprawy::zrodloTestowe.tekst(id, w);
#if CPAP_SILNIK_REGUL
    if (t.empty()) t = reguly::tekst(reguly::zestawWbudowany(), id, w, trybHist());
#endif
    return t;
  }
  // S5 z E-05 krytycznym: teksty 6.4 z zestawu, {x} z wyniku, a bez wyniku (ekran z wpisu dziennika w slocie nocnym)
  // z bieżącego M-25
  void tekstyE05B5(DaneDnia& d) {
    reguly::Wynik w;
    if (const reguly::Wynik* wk = e05KrytyczneB5()) w = *wk;
    else {
      w.id = "E-05";
      w.wariant = "krytyczne";
      w.zmienne["x"] = procentBat >= 0 ? std::to_string(procentBat) : "–";
    }
    d.e05Tytul = tekstB5("E-05.tytul", w);
    d.e05Powod = tekstB5("E-05.powod", w);
    d.e05Krok = tekstB5("E-05.krok", w);
  }
  // zestaw do progów wzorców (Δ, okresy „Omówione”): atrapa testów albo zestaw wbudowany
  const reguly::Zestaw* zestawB11() {
    const reguly::Zestaw* z = sprawy::zrodloTestowe.zestaw;
#if CPAP_SILNIK_REGUL
    if (!z) z = &reguly::zestawWbudowany();
#endif
    return z;
  }
  // wariant potwierdzenia „Omówione” (5.12.7) sprawy z ekranu dnia, jako liczba dla ekranów (ekrany.h)
  int wariantOmowieniaB11(const sprawy::SprawaL& s, const std::string& dzis) {
    if (s.kluczWzorca.empty()) return -1;
    switch (wzorce::wariantOmowienia(b5.pamiec, s.kluczWzorca, dzis, zestawB11())) {
      case wzorce::WariantOmowienia::ZWYKLE: return 1;
      case wzorce::WariantOmowienia::A_PIERWSZE: return 2;
      case wzorce::WariantOmowienia::A_DRUGIE_BEZ_POPRAWY: return 3;
      case wzorce::WariantOmowienia::A_KOLEJNE_BEZ_POPRAWY: return 4;
      case wzorce::WariantOmowienia::NAJPIERW_RAPORT: return 5;
      case wzorce::WariantOmowienia::NIEDOSTEPNE: break;
    }
    return -1;
  }
  const sprawy::SprawaL* sprawaB5(const std::string& kl) {
    for (auto& x : b5.sprawy)
      if (x.klucz == kl) return &x;
    return nullptr;
  }
  // R-PIE-13: termin pytań porannych 11:00, przy imporcie ostatniej nocy po 09:00 import + 2 h (najpóźniej 23:30)
  int terminRanoB5(const std::string& dzis) {
    const Noc* n = nocZDnia(hist, dzis);
    if (!n || n->import == 0) return 11 * 60;
    std::string d;
    int m;
    dataIMinuta(n->import, d, m);
    return d == dzis ? terminOdpowiedzi(m) : 11 * 60;
  }
  // Pilność A zawsze widoczna (5.12.2, przypadek 1 z 7.8): na S5, S10 i ekranie tymczasowym linia grupy 0 z kolejki.
  // zBaterii (S10 i ekran tymczasowy): bez linii grupy 0 linia E-05 krytycznego z dnia pełnego ekranu A
  // (`Naładuj ekran: bateria {x}%.`, R-PIE-07, ostrzeżenie R-EKR-52): pełnego ekranu A tu nie ma, a S5 baterii ustąpiło
  // mu w wyborze, więc bez tej linii bateria krytyczna znikałaby z ekranu. Na S5 (E-01, E-03) bez zmian (R1).
  void liniaPilnaB5(DaneDnia& d, const std::string& dzis, bool zBaterii = false) {
    if (!silnikDzis(dzis) || b5.linie.empty()) return;
    const kolejka::LiniaUwagi* l = nullptr;
    if (b5.linie.front().grupa == GrupaLinii::PILNOSC_A_SENNOSC) l = &b5.linie.front();
    else if (zBaterii)
      for (auto& x : b5.linie)
        if (x.zrodlo == "E-05/krytyczne") l = &x;
    if (!l) return;
    kolejka::ustawLinie(d, {*l});
    pokazanieB11(kolejka::kluczPokazaniaLinii(*l), dzis);
    log("linia uwagi: %s (%s)", d.linia.c_str(), l->zrodlo.c_str());
    if (l->grupa == GrupaLinii::PILNOSC_A_SENNOSC) buzzerB5(dzis);
  }
  // (R-PIE-08): ekran albo linia rzeczywiście stanęły na ekranie; pamięć reguł zapisana od razu (wpis `pokaz~…`)
  void pokazanieB11(const std::string& co, const std::string& dzis) {
    bool zm = sprawy::zapiszPokazanie(b5.pamiec, co, dzis);
    // R-SKI-41: wybór linii C tej niedzieli zapamiętany (`pokaz~linia:C`: dzień i klucz)
    if (co.compare(0, 6, "linia:") == 0)
      if (const sprawy::SprawaL* s = sprawaB5(co.substr(6)))
        if (s->jestL() && s->pilnosc == reguly::Pilnosc::C) {
          reguly::PamiecReguly& c = b5.pamiec[sprawy::kluczPokazania("linia:C")];
          if (c.ostatnio != dzis || c.stanWzorca != s->klucz) {
            c.ostatnio = dzis;
            c.stanWzorca = s->klucz;
            zm = true;
          }
        }
    if (zm) zapiszPamiecRegul();
  }
  // R-BUZ-01, R-BUZ-02, R-BUZ-02b: wejścia w A zagrane zapamiętane w NVS (`cpap`/`buz_a`, znaczniki rozdzielone `|`)
  void buzzerB5(const std::string& dzis) {
    if (drzemkaTrwaB9()) return;  // R-KON-56, w drzemce z maską buzzer milczy
    std::string zap = a.nvs.tekst("cpap", "buz_a", "");
    std::set<std::string> zagrane;
    for (size_t i = 0; i < zap.size();) {
      size_t j = zap.find('|', i);
      if (j == std::string::npos) j = zap.size();
      if (j > i) zagrane.insert(zap.substr(i, j - i));
      i = j + 1;
    }
    // cisza nocna i dźwięk z konfiguracji (R-KONF-30, R-BUZ-03): `buzzer=` i `cisza=` w /konfiguracja.txt
    // (formularz U-03); bez wiersza NVS `cpap`/`cisza_od`, `cisza_do` (minuta doby + 1) i `dzwiek` (0 = wyłączony) z 
    // etapu 1, a bez nich wartości domyślne 5.6.4 (22:00–08:00, dźwięk włączony).
    const UstawieniaDnia& ust = ustawieniaB9();
    int od = ust.ciszaOd, doo = ust.ciszaDo;
    if (od < 0 || doo < 0) {
      od = (int)a.nvs.liczbaU("cpap", "cisza_od", 0) - 1;
      doo = (int)a.nvs.liczbaU("cpap", "cisza_do", 0) - 1;
    }
    if (od < 0 || od >= 24 * 60) od = kolejka::CISZA_OD;
    if (doo < 0 || doo >= 24 * 60) doo = kolejka::CISZA_DO;
    bool dzwiek = ust.buzzer >= 0 ? ust.buzzer != 0 : a.nvs.liczbaU("cpap", "dzwiek", 1) != 0;
    kolejka::DecyzjaBuzzera d = kolejka::buzzerPilnosci(b5.sprawy, zagrane, dzis, minutaDoby(), dzwiek, od, doo);
    if (d.odlozony) log("buzzer pilności A: odłożony do końca ciszy nocnej (R-BUZ-02b)");
    if (!d.graj) return;
    a.dzwiek.pisk(kolejka::SYGNALY_A);
    std::string nowy;  // tylko znaczniki spraw, które wciąż trwają, plus nowe
    for (auto& x : b5.sprawy) {
      std::string z = kolejka::znacznikWejscia(x, dzis);
      if (zagrane.count(z) || std::find(d.znaczniki.begin(), d.znaczniki.end(), z) != d.znaczniki.end())
        nowy += (nowy.empty() ? "" : "|") + z;
    }
    a.nvs.ustawTekst("cpap", "buz_a", nowy);
    log("buzzer pilności A: %d sygnały (%s)", kolejka::SYGNALY_A, d.znaczniki.front().c_str());
  }

  DaneDnia daneDnia(const std::string& dzis) {
    DaneDnia d;
    d.dzis = dzis;
    d.hist = &hist;
    d.stany = &stany(dzis);
    d.norma = d.stany->norma;
    d.abWersja = r.ab.wersja;
    d.httpKod = httpKod;
    d.logowanieOdrzucone = r.logowanieOdrzucone;  // E-01 od 2. nocy: rytm ponawiania w powodzie (R-AW-24, R-AW-35)
    std::string data;
    int minuta;
    if (r.ab.ma) {
      dataIMinuta(r.ab.pobrano, data, minuta);
      d.ostatnieDane = kiedyOdczyt(data, minuta, dzis);
      d.abData = data;
      d.abMinuta = minuta;
    }
    dataIMinuta(r.ab.bladOd, data, minuta);
    d.bladOd = kiedyOdczyt(data, minuta, dzis);
    d.bladData = data;  // licznik nocy E-01 bez udanego odczytu, jak plan
    d.godzinaDanych = r.ab.ma ? godzina(r.ab.pobrano) : "";
    d.sypNoc = sypialniaZDnia(sypNoce, dzis);
    d.sypNorma = liczNormeSypialni(sypNoce, dzis, nocePozaDomem(czynniki));
    uint32_t ep = zegar() ? teraz() : 0;
    d.pogoda = pogodaTeraz(r.oto, ep);
    d.przyLozku = stopkaCzujnika(tempStopki(), wilgStopki(), co2Teraz(r.oto, ep));
    return d;
  }

  // ---------- stan ostatniej nocy przy pobieraniu (5.1.4, src/stan_nocy.h) ----------
  const stan_nocy::FazaNocy& fazaNocy() {
    if (!fazaWczytana) {
      faza = stan_nocy::odczytStanu(a.nvs.tekst("cpap", "sn_noc", ""));
      fazaWczytana = true;
    }
    return faza;
  }
  // Przejście po obserwacji tego wybudzenia; rekord w historii ma pierwszeństwo. Zapis w NVS tylko przy zmianie.
  void obserwujNoc(stan_nocy::Obserwacja o) {
    std::string dzis = dzisiaj();
    if (dzis.empty()) return;
    stan_nocy::FazaNocy przed = fazaNocy();
    faza = stan_nocy::przejdz(przed, dzis, minutaDoby(), o, nocZDnia(hist, dzis));
    if (faza != przed) {
      a.nvs.ustawTekst("cpap", "sn_noc", stan_nocy::zapisStanu(faza));
      log("stan nocy %s: %s → %s", dzis.c_str(), przed.data == dzis ? stan_nocy::nazwa(przed.stan) : "-",
          stan_nocy::nazwa(faza.stan));
    }
  }

  void zapiszStanNocy() {
    a.nvs.ustawTekst("cpap", "noc_w", stanNocy.wieczor);
    a.nvs.ustawFlage("cpap", "noc_s9", stanNocy.s9);
    a.nvs.usun("cpap", "noc_n");  // licznik rotacji z wersji prototypowej
  }

  // Decyzja o S9 tej nocy; zapisz: pierwsza decyzja tej nocy trafia do NVS.
  DecyzjaNocy nocTeraz(bool zapisz) {
    std::string dzis = dzisiaj();
    int m = minutaDoby();
    std::string w = wieczorNocy(dzis, m);
    bool dzien = ekran() == Ekran::DZIEN;
    bool trwa = ekranDniaTrwa(ekran(), w, stanNocy, s5Teraz());
    DecyzjaNocy d = decyzjaNocy(dzis, m, dzien, trwa, stanNocy, Z_PROPOZYCJAMI, humorB32());  // R-KON-39
    if (zapisz && d.nowa) {
      stanNocy = poDecyzji(d, stanNocy);
      zapiszStanNocy();
      log("noc %s: %s", d.wieczor.c_str(), d.nocny ? "ekran nocny" : "bez ekranu nocnego (R-KON-35)");
    }
    return d;
  }

  void rysujNocny(const DecyzjaNocy& dn0) {
    DecyzjaNocy dn = dn0;
    if (humorB32() == Humor::WYLACZONY) {  // R-KON-39 (także po zmianie ustawienia już po decyzji tej nocy)
      dn.wybor.scena.clear();
      dn.wybor.zdanie.clear();
    }
    Norma nm = norma(dzisiaj());
    int zwykly = nm.m01.jest() ? (int)lround(nm.m01.med) : -1;
    bool nowe = dn.wieczor != r.faktyNocy.wieczor;
    // godzina maski tylko przy pierwszym rysowaniu tej nocy (potem z RTC)
    int maska = nowe ? maskaOd(odczytMaski()) : -1;
    DaneNocy d = daneNocy(dn, r.faktyNocy, temp, wilg, zwykly, maska);
    ustawIkoneAktualizacji(false);  // R-EKR-61: bez ikony na S9
    rysujDobranoc(plotno(), d);
    log("ekran nocny (%s), n %ld, scena %s", nowe ? "fakty z tej chwili" : "fakty z pierwszego rysowania", dn.wybor.n,
        dn.wybor.scena.c_str());
  }

  // ---------- slot dzienny, obrazek dnia, ekrany CO2 (src/slot_dzienny.h, src/obrazki.h,
  // src/kontekst.h). Logika i rysowanie w plikach; tu tylko zebranie wejścia z RTC, NVS, plików i konfiguracji. ----
  Prognoza prognozaB9_;
  bool prognozaWczytana_ = false, prognozaDoZapisu_ = false;
  UstawieniaDnia ustawieniaB9_;
  bool ustawieniaWczytane_ = false;
  // współrzędne pogody i wschodu słońca: wpis z formularza U-03 (`miasto=`) ma
  // pierwszeństwo przed `POGODA_LAT` i `POGODA_LON` z secrets.h; bez wpisu jak dotąd (R-KONF-24). Plik czytany raz na
  // wybudzenie, więc nowe współrzędne działają od następnego wybudzenia (prognoza i S-K-37 biorą je przy najbliższym użyciu).
  double pogodaLatB32() {
    const UstawieniaDnia& u = ustawieniaB9();
    return std::isnan(u.pogodaLat) ? k.pogodaLat : u.pogodaLat;
  }
  double pogodaLonB32() {
    const UstawieniaDnia& u = ustawieniaB9();
    return std::isnan(u.pogodaLon) ? k.pogodaLon : u.pogodaLon;
  }
  bool miastoB9() { return !std::isnan(pogodaLatB32()) && !std::isnan(pogodaLonB32()); }
  // humor z `humor=` (5.6.4); bez wiersza `normalny`
  Humor humorB32() {
    int h = ustawieniaB9().humor;
    return h == 0 ? Humor::WYLACZONY : h == 1 ? Humor::OSZCZEDNY : Humor::NORMALNY;
  }
  const Prognoza& prognozaB9() {
    if (!prognozaWczytana_) {
      prognozaWczytana_ = true;
      std::vector<std::string> w = wczytaj("prognoza");
      if (!w.empty() && !prognozaZWierszy(w, prognozaB9_)) log("prognoza: plik nieczytelny");
      if (prognozaB9_.jest()) prognozaB9_.godzinaPobrania = godzina(prognozaB9_.pobrano);
    }
    return prognozaB9_;
  }
  // R-KON-52: dwa zapytania do Open-Meteo (prognoza i powietrze), bez danych terapii (ADR-0001); błąd = stara prognoza.
  // Zapis w magazynie dopiero po wyłączeniu Wi-Fi (zapiszPrognozeB9, 7.1 krok 10).
  void pobierzPrognozeB9() {
    if (!miastoB9() || !a.siec.polaczona() || !zegar()) return;
    ZadanieHttp z;
    z.limitMs = 8000;
    z.maksBajtow = 16384;
    z.cialoTylko200 = true;
    z.url = urlPrognozy(pogodaLatB32(), pogodaLonB32());
    OdpowiedzHttp o1 = a.siec.get(z);
    Prognoza p;
    if (o1.kod != 200 || !parsujPrognoze(o1.cialo, p)) { log("prognoza godzinowa: błąd %d", o1.kod); return; }
    z.url = urlPowietrza(pogodaLatB32(), pogodaLonB32());
    OdpowiedzHttp o2 = a.siec.get(z);
    if (o2.kod != 200 || !parsujPowietrze(o2.cialo, p)) log("prognoza powietrza: błąd %d (bez PM2.5 i pyłku)", o2.kod);
    p.pobrano = teraz();
    p.godzinaPobrania = godzina(p.pobrano);
    prognozaB9_ = p;
    prognozaWczytana_ = true;
    prognozaDoZapisu_ = true;
    log("prognoza godzinowa: pobrana %s (%s)", p.godzinaPobrania.c_str(), p.data.c_str());
  }
  void zapiszPrognozeB9() {
    if (!prognozaDoZapisu_) return;
    prognozaDoZapisu_ = false;
    zapiszFormat("prognoza", mag.sciezkaRoku("prognoza", ""), wierszePrognozy(prognozaB9_));
  }
  // ustawienia slotu (daty wymiany 5.9.2, `Pyłki w dzień` 5.6.4) z /konfiguracja.txt do formularza 
  const UstawieniaDnia& ustawieniaB9() {
    if (!ustawieniaWczytane_) {
      ustawieniaWczytane_ = true;
      std::string t;
      if (fsOk() && a.pliki.czytaj("/konfiguracja.txt", t)) {
        ustawieniaB9_ = parsujUstawieniaDnia(t);
        for (auto& b : ustawieniaB9_.bledy) log("konfiguracja: pominięty wiersz '%s'", b.c_str());
        if (ustawieniaB9_.profilPubliczny) log("konfiguracja: profil publiczny (5.6.4, R-EKR-51)");
        const int zr = ustawieniaB9_.nocZrodloSkuteczne();
        const char* jak = ustawieniaB9_.nocZrodlo < 0 ? "domyślnie" : "noc_zrodlo=plik";
        if (zr == 1 && ustawieniaB9_.nocJsonZly)
          log("konfiguracja: noc z plików (%s), ale lista noc_json nieczytelna: noce z raportu JSON (ADR-0036 pkt 5)",
              jak);
        else if (zr == 1)
          log("konfiguracja: noc z plików sesji (%s, ADR-0036), z raportu JSON %d metryk", jak,
              (int)ustawieniaB9_.nocJson.size());
        else if (zr == 0)
          log("konfiguracja: noc z raportu JSON bez porównania z plikami (noc_zrodlo=json)");
      }
    }
    return ustawieniaB9_;
  }
  // „Profil publiczny” (5.6.4, R-EKR-51) z `/konfiguracja.txt` (`profil=publiczny|prywatny`), domyślnie prywatny
  // Teksty AHI i PM2.5 bez zamiany w obu profilach (5.4.14). Plik czytany raz na wybudzenie: zmiana w trakcie oglądania działa od następnego
  // przycisku albo wybudzenia (R-EKR-51 fail-path: otwarty ekran zostaje, ▶ na S8 prowadzi wtedy do K1).
  bool profilB18() { return ustawieniaB9().profilPubliczny; }
  // Seria CO2 nocy z datą poranka `dzien`: rekord sypialni (wersja 2) albo, przed jego zamknięciem o 06:00, bufor nocy.
  Co2Nocy co2NocyB9(const std::string& dzien) {
    if (const NocSypialni* n = sypialniaZDnia(sypNoce, dzien)) return co2ZNocy(n);
    if (dzien == r.buforSyp.data) {
      NocSypialni n;
      co2ZBufora(r.buforSyp, n);
      return co2ZNocy(&n);
    }
    return Co2Nocy();
  }
  bool wpisDzis(const char* typ, const std::string& dzis) {
    for (auto& x : dziennik)
      if (x.data == dzis && x.typ == typ) return true;
    return false;
  }
  // R-KON-57: po drzemce (nowa sesja po imporcie nocy, `rop` ≠ 1) raport doby bez zapisu rekordu nocy; stan w NVS.
  // Wołane z pobierz() przy ocenie slotu dziennego (tylko `/api/status`), w tej samej sesji Wi-Fi.
  void drzemkaB9(const StatusAB& st) {
    std::string dzis = dzisiaj(), d;
    int m = 0;
    if (dzis.empty() || st.rop == 1 || !prefiksSesji(st.prefiks, d, m) || d != dzis) return;
    int imp = sekundaDzis(r.importNocyEp);
    if (imp < 0 || m * 60 <= imp || a.nvs.tekst("cpap", "b9_pref", "") == st.prefiks) return;
    std::string t;
    int kod = abGetRaz(sciezkaNocy(nrDnia(dzis) - 1), t, LIMIT_STATUSU_SLOTU_MS);
    std::vector<SesjaDoby> ses;
    float p95 = NAN;
    if (kod != 200 || !sesjeZRaportu(t, ses, p95)) { log("drzemka: raport doby niedostępny (%d), ponowię", kod); return; }
    Drzemka dr = liczDrzemke(ses, r.importNocyEp);
    const Noc* nc = nocZDnia(hist, dzis);
    dr.wyciekP95 = wyciekDrzemkiZDoby(p95, nc ? nc->wp95 : NAN);
    a.nvs.ustawTekst("cpap", "b9_drz", zapiszDrzemke(dzis, dr));
    a.nvs.ustawTekst("cpap", "b9_pref", st.prefiks);
    log("drzemka: %d min, wyciek %s (R-KON-57, rekord nocy bez zmian)", dr.minut, calkowita(dr.wyciekP95).c_str());
  }
  Drzemka drzemkaDzisB9(const std::string& dzis) {
    Drzemka d;
    parsujDrzemke(a.nvs.tekst("cpap", "b9_drz", ""), dzis, d);
    return d;
  }
  // R-KON-56: w slocie dziennym przy `rop` = 1 (drzemka z maską) bez odświeżania ekranu i bez buzzera
  bool drzemkaTrwaB9() {
    if (!tylkoStatusTeraz || r.ab.rop != 1 || !zegar()) return false;
    int m = minutaDoby();
    return sekundaDzis(r.slotDziennyEp) >= 0 &&
           m < koniecSlotuDziennego(slot_wieczorny::oknoZHistorii(hist, dzisiaj()));  // koniec z R-KON-09
  }
  // prośba dziś (7.4, R-PIE-10): ekran dnia z krokiem rano (grupy R3…R5d, wzorzec B albo „problem” z kolejki)
  // albo linia zaproszenia na ekranie
  bool prosbaDzisB9(const std::string& dzis) {
    for (auto& x : dziennik)
      if (x.data == dzis && x.typ != "SK01" && x.typ != "SK02" && grupaRano(x.typ) == 0 && x.typ != "S5" &&
          x.typ != "SK12")
        return true;
    if (const sprawy::SprawaL* e = sprawaB5(b5.kluczEkranu))
      if (e->pilnosc == reguly::Pilnosc::B || e->ton == sprawy::Ton::PROBLEM) return true;
    const WpisCzynnikow* z = wpisZDnia(czynniki, dzis);
    return z && z->pokazano;
  }
  int wieczoryZKrokiemB9(const std::string& dzis) {
    std::set<std::string> d;
    for (auto& x : dziennik)
      if (x.typ.size() == 4 && x.typ.compare(0, 3, "SK0") == 0 && x.data < dzis && x.data >= dodajDni(dzis, -7))
        d.insert(x.data);
    return (int)d.size();
  }

  // stan tego wybudzenia (norma i okresy do wskaźników sk::Wejscie, log trybu próbnego raz na ekran i dzień)
  struct StanB16 {
    Norma norma;
    Stany stany;  // ADR-0039: stany odchylenia dnia do kart S-K-40…S-K-47
    std::vector<infekcja::Okres> okresy;
    sprzet::Elementy sprzet;
    std::set<std::string> zalogowane;
    uint32_t sk31 = 0;  // ekran dnia to S-K-31: element + 1 (◀ 2 s, R-MAS-01); NVS `cpap`/`b16_sk31` po rysujDzien()
  } b16;
  // ---------- moduły SPRZĘT i INFEKCJA, pozostałe ekrany S-K (src/sprzet.h, src/infekcja.h, src/kontekst_sk.h) ----------
  bool asvB16() const {
    reguly::Tryb t = trybHist();
    return t == reguly::Tryb::ASV || t == reguly::Tryb::ASVAUTO;
  }
  std::vector<std::string> odpowiedziB16(sprawy::Pytanie p, bool tylkoTak) const {
    std::vector<std::string> v;
    for (auto& x : odpowiedzi)
      if (x.pytanie == p && (!tylkoTak || x.odp == 't')) v.push_back(x.data);
    return v;
  }
  // Okresy infekcji z odpowiedzi „tak” na S-K-30 (5.10.3); norma okresu bez nocy okresu (czynnik „katar/infekcja”).
  // pamięć podręczna na wybudzenie; klucz: data, odpowiedzi „tak”, czynniki i skrót historii (bez kopii)
  std::string kluczOkresowB16_;
  std::vector<infekcja::Okres> okresyB16_;
  std::string skrotHistB16() const {
    double s = 0;
    for (auto& n : hist) s += (std::isnan(n.m13) ? 0 : n.m13) + (n.wazna ? 1 : 0) * 0.001 + n.m01 * 1e-6;
    char b[64];
    snprintf(b, sizeof b, "%u;%s;%.6f", (unsigned)hist.size(), hist.empty() ? "" : hist.back().data.c_str(), s);
    return b;
  }
  const std::vector<infekcja::Okres>& okresyB16(const std::string& dzis) {
    std::vector<std::string> tak = odpowiedziB16(sprawy::Pytanie::INFEKCJA, true);
    if (tak.empty()) {  // najczęstszy przypadek: bez „tak” nie ma okresów
      okresyB16_.clear();
      kluczOkresowB16_.clear();
      return okresyB16_;
    }
    std::set<std::string> wyk = wykluczoneZNormy(czynniki);
    std::string kl = dzis + "|" + skrotHistB16() + "|" + std::to_string(asvB16());
    for (auto& x : tak) kl += ";" + x;
    for (auto& x : wyk) kl += "," + x;
    if (kl != kluczOkresowB16_) {
      okresyB16_ = infekcja::okresy(hist, tak, wyk, dzis, asvB16());
      kluczOkresowB16_ = kl;
    }
    return okresyB16_;
  }
  // R-CZY-05, R-INF-05: noce wyłączone z normy = czynniki nocy i noce okresów infekcji (z datą wywołującego)
  std::set<std::string> wykluczoneB16(const std::string& dzis) {
    std::set<std::string> w = wykluczoneZNormy(czynniki);
    if (dataOk(dzis))
      for (auto& d : infekcja::noceOkresow(okresyB16(dzis), dzis)) w.insert(d);
    return w;
  }
  sprzet::Elementy sprzetB16() {
    std::vector<std::string> inf;  // ta sama lista nocy wyłączonych co w pytaniu R-MAS-12
    std::string dzis = dzisiaj();
    if (dataOk(dzis))
      for (auto& x : infekcja::noceOkresow(okresyB16(dzis), dzis)) inf.push_back(x);
    return sprzet::zOdpowiedziami(ustawieniaB9().sprzet, hist, odpowiedziB16(sprawy::Pytanie::PODUSZKA, true), inf);
  }
  bool wynikB16(const char* id) const {
    for (auto& w : b5.wyniki)
      if (w.id == id && w.wariant.empty() && w.oceniona && w.spelniona && w.stan != reguly::StanReguly::ZABLOKOWANA)
        return true;
    return false;
  }
  sk::Wejscie wejscieB16(const std::string& dzis, const DaneDnia& d) {
    sk::Wejscie w;
    w.dzis = dzis;
    w.hist = &hist;
    b16.norma = stany(dzis).norma;
    w.norma = &b16.norma;
    b16.stany = stany(dzis);  // ADR-0039: karty odchyleń S-K-40…S-K-47
    w.stany = &b16.stany;
    w.tryb = trybHist();
    w.sypialnia = &sypNoce;
    w.tempTeraz = tempStopki();  // przy ocenie bez Wi-Fi liczba na S-K jak na wyświetlonej klatce …
    w.wilgTeraz = wilgStopki();
    const Prognoza& pr = prognozaB9();
    w.prognoza = zegar() && prognozaWazna(pr, teraz()) ? &pr : nullptr;
    w.pylki = &ustawieniaB9().pylki;
    if (ustawieniaB9().uwagaOMasce > 0) w.uwagaOMasce = ustawieniaB9().uwagaOMasce;  // R-KONF-42, bez wiersza 10
    w.dziennik = &dziennik;
    w.airbridge = !brakDanychAirBridge(wejscieDnia(false));  // AirBridge odpowiada; bateria (E-05) tu nie należy
    w.e14 = wynikB16("E-14");
    w.e21 = wynikB16("E-21");
    w.e23 = wynikB16("E-23");
    for (auto& x : b5.wyniki)
      if (x.id == "L-07" && x.oceniona && x.spelniona && x.stan == reguly::StanReguly::AKTYWNA) w.l07Aktywne = true;
    b16.okresy = okresyB16(dzis);
    w.okresy = &b16.okresy;
    w.okresInfekcji = infekcja::okresDzis(b16.okresy, dzis) != nullptr;
    b16.sprzet = sprzetB16();
    w.sprzet = &b16.sprzet;
    w.pozaDomem = nocePozaDomem(czynniki);
    for (auto& n : hist)
      if (n.m18 >= 0) w.startyM18[n.data] = n.m18;
    reguly::Odpowiedzi odp = sprawy::odpowiedziSilnika(odpowiedzi);
    auto s = odp.sennosc.find(dzis);
    w.sennoscTak = s != odp.sennosc.end() && s->second;
    w.uspokojenie = b5.stany.uspokojenie;
    w.liniaZdrowiaNaEkranie = !d.linia.empty() && d.liniaOstrzezenie;
    w.tylkoRegulyBezOceny = b5.stany.niespokoj == Stany::Niespokoj::REGULY;
    w.tylkoOcenyWylaczone = b5.stany.niespokoj == Stany::Niespokoj::OCENY;
    w.tylkoBrakDanych = b5.stany.niespokoj == Stany::Niespokoj::DANE;  //
    w.brakDanychWyciek = w.tylkoBrakDanych && b5.stany.bezOcenZWycieku;  // 5.4.13 pkt 7: przyczyna to wyciek
    // R-EKR-32: krok spraw tylko przy stronie spraw (R-KOK-19). Z wyników silnika już policzonych dziś (bez wołania
    // silnikDzis(): w trakcie liczenia wołałby sam siebie, a wcześniej policzyłby wyniki bez okoliczności ranka)
    if (!wSilniku) {
      bool zSilnikiem = b5.dzis == dzis && !b5.wyniki.empty();
      w.bezSilnika = !zSilnikiem;
      w.bezKartSpraw = !zSilnikiem || kartySprawB5(dzis) == 0;
    }
    w.prosbaDzis = prosbaDzisB9(dzis) && !sk::wpisDzis(dziennik, dzis, true);
    w.wieczorowZKrokiem7 = wieczoryZKrokiemB9(dzis);
    if (stopkaZPamieci) {  // … a wybór ekranu ze świeżego odczytu: inny zestaw S-K (próg przekroczony) → świeży odczyt
      sk::Wejscie f = w;
      f.tempTeraz = temp;
      f.wilgTeraz = wilg;
      std::vector<sk::Tresc> stare = sk::wieczor(w), nowe = sk::wieczor(f);
      bool ten = stare.size() == nowe.size();
      for (size_t i = 0; ten && i < stare.size(); i++) ten = stare[i].id == nowe[i].id;
      if (!ten) return f;
    }
    return w;
  }
  // Okoliczności wzorców i bramki 7.3 z modułów; wołane w silnikDzis po ocenie silnika.
  void bramkiB16(const std::string& dzis, wzorce::Okolicznosci& ow) {
    std::vector<infekcja::Okres> o = okresyB16(dzis);
    const infekcja::Okres* ok = infekcja::okresDzis(o, dzis);
    ow.okresInfekcji = ok != nullptr;
    sprzet::Elementy el = sprzetB16();
    Norma nm = stany(dzis).norma;
    sprzet::WejscieSprzetu ws;
    ws.dzis = dzis;
    ws.hist = &hist;
    ws.norma = &nm;
    ws.el = &el;
    ws.dziennik = &dziennik;
    const char* pw = "";
    ow.l09Sprzet = sprzet::blokadaL09(ws, &pw);
    if (ow.l09Sprzet) log("L-09 wstrzymane: %s", pw);
    // bramka `tylko_okres_infekcji` z katalogu (L-05 wariant okresu, R-SKI-23): poza okresem „nie oceniam”
    if (!ok)
      if (const reguly::Zestaw* z = zestawB11())
        for (auto& w : b5.wyniki)
          if (w.oceniona)
            for (auto& wk : z->katalog)
              if (wk.id == w.id && wk.wariant == w.wariant && wk.bramki == "tylko_okres_infekcji") {
                w.oceniona = false;
                w.powodNieoceny = "poza okresem infekcji (R-SKI-23)";
              }
    if (ok) {  // R-INF-02: E-20, E-21, E-22 wstrzymane w okresie (bramka 7.3): „nie oceniam”, nigdy „w porządku”
      for (auto& w : b5.wyniki)
        if ((w.id == "E-20" || w.id == "E-21" || w.id == "E-22") && w.oceniona) {
          w.oceniona = false;
          w.powodNieoceny = "okres infekcji (R-INF-02)";
        }
      log("okres infekcji od %s (odpowiedź %s)", ok->od.c_str(), ok->odpowiedz.c_str());
    } else {  // karencja R-INF-04.karencja w logu, żeby odróżnić ją od normy, która dogoniła oddechy
      std::string kk = infekcja::koniecKarencji(o, dzis);
      if (!kk.empty()) log("infekcja: karencja do %s (R-INF-04.karencja, bez S-K-30 i nowego okresu)", kk.c_str());
    }
  }
  // Ekran z treści: karta albo ekran danych według 5.7.3 (ekranKarty), wpis w dzienniku przy pierwszym
  // pokazaniu dnia (limity 7.4, licznik pokazań S-K-31). false = tylko log (tryb próbny).
  bool pokazB16(sk::Tresc t, const DaneDnia& dd, const Naglowek& nag, const std::string& dzien) {
    if (t.doLogu) {
      if (!b16.zalogowane.count(t.id + dzien)) log("tryb próbny: %s w logu (%s, wartość %d)", t.id.c_str(), dzien.c_str(), t.wartosc);
      b16.zalogowane.insert(t.id + dzien);
      return false;
    }
    const std::string dalej = stopkaDalejPortretu();  // R-POR-24: rano ▶ otwiera Portret → `▶ portret nocy`
    t.t.linia = dd.linia;
    t.t.liniaA = dd.liniaA;
    t.t.liniaOstrzezenie = dd.liniaOstrzezenie;
    const WpisHumoru* wp = nullptr;
    for (auto& x : dziennik)
      if (x.data == dzien && x.typ == t.typ) wp = &x;
    Wybor wb;
    if (wp) {
      wb.tryb = wp->tryb;
      wb.scena = wp->scena;
      wb.zdanie = wp->zdanie;
    } else if (t.karta) {
      EkranDnia ed = ekranKarty(t.typ, t.sprawa, dzien, dziennik, t.t.krok);
      TrescEkranu tt = t.t;
      tt.tytul += ".";
      KontekstHumoru kh;
      kh.dzis = dzien;
      kh.humor = humorB32();
      kh.liniaUwagi = !dd.linia.empty();
      kh.zPropozycjami = Z_PROPOZYCJAMI;
      kh.sceny = sceny();
      wb = wybierzTryb(ed, kartaMiesci(tt), dziennik, kh);
      dopiszDziennik({dzien, t.typ, ed.klucz, wb.tryb, wb.scena, wb.zdanie});
      log("ekran %s (%s): %s", t.id.c_str(), t.grupa.c_str(), wb.tryb == 'K' ? "karta" : wb.dlaczego);
    } else {
      dopiszDziennik({dzien, t.typ, t.sprawa, 'D', "", ""});
      log("ekran %s (%s): ekran danych", t.id.c_str(), t.grupa.c_str());
    }
    if (wb.tryb == 'K' && dd.linia.empty()) {
      TrescEkranu tt = t.t;
      tt.tytul += ".";
      tt.etykieta = wersaliki(tt.etykieta);
      tt.tonProblem = t.problem;
      tt.stopkaPrawo = stopkaCzujnika(tempStopki(), wilgStopki());
      tt.stopkaLewo = dd.pogoda;  // lewa część stopki jak na ekranie danych (bez zdania budzika od 10.11.2026)
      // R-POR-24 (decyzja właściciela projektu 11.11.2026): ▶ otwiera Portret: skrócony podpis `▶ portret` przed odczytem czujnika,
      // odczyt zostaje
      if (!dalej.empty()) tt.stopkaPrawo = prawoZPortretem(tt.stopkaPrawo);
      dopasujStopke(tt.stopkaLewo, tt.stopkaPrawo);  // R-EKR-10: ustępują opis pogody, miasto, dopisek CO2
      if (t.id == "S-K-31") {  // R-PRZ-04: akcja niestandardowa w stopce (kanwa SK31-maska)
        tt.stopkaLewo = sprzet::STOPKA_WYMIANY_LEWO;
        tt.stopkaPrawo = dalej.empty() ? std::string(sprzet::STOPKA_WYMIANY_PRAWO) : dalej;
      }
      rysujKarte(plotno(), nag, tt, wb.scena);
    } else if (t.id == "S-K-31") {
      sk::rysuj(plotno(), nag, t, sprzet::STOPKA_WYMIANY_LEWO,
                dalej.empty() ? std::string(sprzet::STOPKA_WYMIANY_PRAWO) : dalej);  // R-PRZ-04 (kanwa SK31-maska)
    } else {
      std::string lw = dd.pogoda, pr = dd.przyLozku;
      if (!dalej.empty()) {  // R-POR-24 (decyzja właściciela projektu 11.11.2026): `▶ portret` przed odczytem czujnika, odczyt zostaje
        pr = prawoZPortretem(pr);
        dopasujStopke(lw, pr);  // R-EKR-10: najpierw ustępuje opis pogody i miasto
      }
      sk::rysuj(plotno(), nag, t, lw, pr);
    }
    b16.sk31 = t.id == "S-K-31" ? (uint32_t)(t.wartosc + 1) : 0;  // ◀ 2 s (R-MAS-01)
    dzienOdswiezalny = false;
    r.dzienTryb = 0;
    return true;
  }
  // Punkt wejścia z rysujDzien. `etap` 0: przed ekranem L-09 z (R4…R5b przed R5c, S-K-31 przed L-09);
  // 1:, przed (tydzień T2, wieczór S-K-05, rano R4…R7 i problem z rana przez cały dzień);
  // 2: po (wieczór S-K-03…S-K-09, W4 z problemem z rana, rano R8 przed slotem dziennym).
  bool rysujB16(const EkranDnia& e, DaneDnia& d, const Naglowek& n, int etap) {
    std::string dzis = dzisiaj();
    if (dzis.empty() || !zegar() || e.typ == "S5" || e.typ == "S10") return false;
    int m = minutaDoby();
    sk::Wejscie w = wejscieB16(dzis, d);
    bool wieczor = slot_wieczorny::wOknie(slot_wieczorny::oknoZHistorii(hist, dzis), m);
    bool slotDzienny = sekundaDzis(r.slotDziennyEp) >= 0 && !k.bezSlotuDziennego && m < PLAN_SLOT_DZIENNY_DO;
    if (etap == 0) {
      // ekran L-09 albo zdarzenia E (S3) jako ekran dnia: kandydat z wyższej grupy 7.2 wygrywa, w tej samej
      // grupie S-K-31 na czele R5c (przed L-09, E-22, E-21) i S-K-17 przed E-23 w R5b
      const sprawy::SprawaL* l = sprawaB5(b5.kluczEkranu);
      if (!l || (l->id != "L-09" && !l->jestE()) || wieczor) return false;
      int rl = sk::ranga(l->id == "L-09" ? std::string("R5c") : l->grupa);
      if (rl >= 99) return false;
      for (auto& t : sk::rano(w)) {
        if (t.doLogu || t.id == "S-K-30") continue;  // S-K-30 idzie jako pytanie (pytanieB16)
        int rt = sk::ranga(t.grupa);
        if (rt < rl || (rt == rl && (t.id == "S-K-31" || (t.id == "S-K-17" && l->id == "E-23"))))
          return pokazB16(t, d, n, dzis);
      }
      return false;
    }
    if (etap == 1 && slot_wieczorny::slotTygodniowy(dzis, m)) {  // T2 (R-KON-08, R-KON-26)
      sk::Tresc t;
      bool jest = sk::tydzien(w, t);
      if (!b16.zalogowane.count("T" + dzis)) {
        b16.zalogowane.insert("T" + dzis);
        log("slot tygodniowy %s: S-K-18 %s; S-K-19…S-K-21 w poniedziałek (R-KON-27)", dzis.c_str(), jest ? "spełniony" : "nie");
      }
      if (jest) return pokazB16(t, d, n, dzis);
    }
    if (wieczor) {
      std::vector<sk::Tresc> v = sk::wieczor(w);
      for (auto& t : v) {
        if (etap == 1 && t.id != "S-K-05") break;  // W1: S-K-05 przed S-K-02 i S-K-01
        if (pokazB16(t, d, n, dzis)) return true;
      }
      if (etap == 2) {  // W4: ekran z rana zostaje (problem poranny z dziennika)
        const WpisHumoru* x = sk::wpisDzis(dziennik, dzis, false);
        if (x)
          for (auto& t : sk::rano(w))
            if (t.typ == x->typ && sk::ranga(t.grupa) <= sk::ranga("R5d")) return pokazB16(t, d, n, dzis);
      }
      return false;
    }
    if (m < 5 * 60) return false;
    int rangaDnia = sk::rangaTypu(e.typ) * 10;
    for (auto& t : sk::rano(w)) {
      if (t.id == "S-K-30") continue;  // S-K-30 idzie jako pytanie tak/nie (b5.pytanie, Pytanie::INFEKCJA)
      if (t.doLogu) { pokazB16(t, d, n, dzis); continue; }
      int r_ = sk::ranga(t.grupa) * 10 + t.kolejnosc;
      bool problem = sk::ranga(t.grupa) <= sk::ranga("R5d");
      bool sk10o = t.typ == "SK10W";  // R-KONF-42: wariant opisowy S-K-10 na czele R8, przed S-K-13 z (7.2)
      if (rangaDnia && r_ >= rangaDnia + (e.typ == "KM" || (sk10o && e.typ == "SK13") ? 1 : 0) &&
          !(e.typ == "KM" && t.id == "S-K-27"))
        return false;
      if (!problem && slotDzienny) return false;  // R6…R9 ustępują slotowi dziennemu (D0)
      if (etap == 1 && sk::ranga(t.grupa) == sk::ranga("R8") && !sk10o) return false;  // R8: po S-K-13
      return pokazB16(t, d, n, dzis);
    }
    return false;
  }
  // Ekran nocny bez S9 (U1) z wpisem wieczornym ten sam ekran co wieczorem.
  bool rysujNocB16(const WpisHumoru& wp, DaneDnia& d, const Naglowek& n) {
    if (!sk::typWieczorny(wp.typ)) return false;
    sk::Wejscie w = wejscieB16(wp.data, d);
    w.prosbaDzis = false;
    for (auto& t : sk::wieczor(w))
      if (t.typ == wp.typ) return pokazB16(t, d, n, wp.data);
    return false;
  }
  // R-MAS-01: „wymienione dziś” zapisuje datę w /konfiguracja.txt (formularz czyta ten sam plik)
  // true = zapis w pliku udany.
  bool wymianaB16(int element) {
    std::string dzis = dzisiaj();
    if (!dataOk(dzis) || element < 0 || element >= EL_N) return false;
    std::string t;
    if (fsOk()) a.pliki.czytaj("/konfiguracja.txt", t);
    std::string nowa = sprzet::zapiszWymiane(t, element, dzis);
    bool ok = fsOk() && a.pliki.zapiszAtomowo("/konfiguracja.txt", nowa);
    if (ok) log("sprzęt: wymiana %s %s (R-MAS-01)", NAZWY_ELEMENTU[element], dzis.c_str());
    else log("sprzęt: zapis wymiany nieudany, data do restartu (R-AW-05)");
    ustawieniaB9_ = parsujUstawieniaDnia(nowa);
    ustawieniaWczytane_ = true;
    return ok;
  }
  // R-MAS-16: pole `Nowy model maski od dziś` w formularzu U-03 (formularz::Wynik::nowyModelMaski): datę wymiany maski
  // = dziś zapisuje formularz w /konfiguracja.txt (jak R-MAS-01), tu tylko data nowego modelu w NVS `cpap`/`sp_model`
  // (zerowanie normy M-07 od tej nocy, histNormyB16(); linia R-MAS-11 rano, liniaNowegoModeluB16()). Wołane po zapisie
  // pliku; `plikZapisany` false = bez zerowania normy (inaczej po restarcie norma M-07 zerowana przy starych datach).
  // `staraMaska` = data wymiany maski sprzed zapisu formularza: razem z poprzednim `sp_model` trafia do `sp_cof`
  // (`;{data maski};{model}`, format z R-MAS-14 sprzed ADR-0041) do cofnięcia tego samego dnia (R-MAS-19).
  void nowyModelZFormularzaB16(bool plikZapisany, const std::string& staraMaska) {
    std::string dzis = dzisiaj();
    if (!dataOk(dzis)) return;
    if (!plikZapisany) {
      log("sprzęt: nowy model maski bez zerowania normy, zapis dat nieudany (R-AW-05)");
      return;
    }
    std::string model = a.nvs.tekst("cpap", "sp_model", "");
    // drugi zapis tego samego dnia nie nadpisuje stanu sprzed pierwszego (cofnięcie wraca do stanu sprzed dziś)
    if (model != dzis) a.nvs.ustawTekst("cpap", "sp_cof", ";" + staraMaska + ";" + model);
    a.nvs.ustawTekst("cpap", "sp_model", dzis);
    log("sprzęt: nowy model maski %s z formularza, norma M-07 od nowa (R-MAS-16)", dzis.c_str());
    b5.dzis.clear();
  }
  // R-MAS-19: pole `Cofnij nowy model z dziś` w formularzu (tylko gdy `sp_model` = dziś): data wymiany maski i `sp_model`
  // sprzed zapisu z `sp_cof`, norma M-07 bez zerowania, bez linii R-MAS-11 rano. true = cofnięte.
  bool cofnijNowyModelZFormularzaB16() {
    std::string dzis = dzisiaj();
    if (!dataOk(dzis) || a.nvs.tekst("cpap", "sp_model", "") != dzis) {
      log("sprzęt: brak nowego modelu maski z dziś do cofnięcia (R-MAS-19)");
      return false;
    }
    std::string c = a.nvs.tekst("cpap", "sp_cof", "");
    size_t i = c.find(';'), j = i == std::string::npos ? i : c.find(';', i + 1);
    std::string maska = j == std::string::npos ? "" : c.substr(i + 1, j - i - 1);
    std::string model = j == std::string::npos ? "" : c.substr(j + 1);
    std::string t;
    if (fsOk()) a.pliki.czytaj("/konfiguracja.txt", t);
    std::string nowa = sprzet::przywrocWymiane(t, EL_MASKA, maska);
    bool ok = fsOk() && a.pliki.zapiszAtomowo("/konfiguracja.txt", nowa);
    ustawieniaB9_ = parsujUstawieniaDnia(nowa);
    ustawieniaWczytane_ = true;
    if (model.empty()) a.nvs.usun("cpap", "sp_model");
    else a.nvs.ustawTekst("cpap", "sp_model", model);
    a.nvs.usun("cpap", "sp_cof");
    b5.dzis.clear();
    log(ok ? "sprzęt: nowy model maski cofnięty (R-MAS-19)" : "sprzęt: nowy model cofnięty, zapis dat nieudany (R-AW-05)");
    return true;
  }
  // R-MAS-01: ◀ 2 s na ekranie `Wymiana zapisana.` przywraca datę wymiany sprzed zapisu (`sp_cofw`: `{element};{data}`,
  // pusta data = wiersz usunięty, R-MAS-06)
  void cofnijWymianeB16() {
    std::string w = a.nvs.tekst("cpap", "sp_cofw", "");
    size_t sc = w.find(';');
    int el = sc == std::string::npos ? -1 : liczbaCalk(w.substr(0, sc).c_str());
    if (el >= 0 && el < EL_N) {
      std::string t;
      if (fsOk()) a.pliki.czytaj("/konfiguracja.txt", t);
      std::string nowa = sprzet::przywrocWymiane(t, el, w.substr(sc + 1));
      if (fsOk() && a.pliki.zapiszAtomowo("/konfiguracja.txt", nowa)) log("sprzęt: wymiana %s cofnięta (R-MAS-01)", NAZWY_ELEMENTU[el]);
      else log("sprzęt: zapis cofnięcia nieudany, daty do restartu (R-AW-05)");
      ustawieniaB9_ = parsujUstawieniaDnia(nowa);
      ustawieniaWczytane_ = true;
    }
    if (!w.empty()) a.nvs.usun("cpap", "sp_cofw");
    b5.dzis.clear();
  }
  // R-MAS-01: 30 s [PRÓG] (sekundDoPowrotu, src/nawigacja.cpp), w slocie nocnym do 05:00 bez `◀ 2 s cofnij` po 30 s
  bool cofniecieWymianyB16() {
    if (!slotNocny(minutaDoby())) return true;
    return zegar() && (long)(teraz() - r.ekranOd) < 30;
  }
  void rysujWymianeZapisanaB16() {
    DanePotwierdzenia p;
    p.naglowek = {"Sprzęt", ""};
    std::string cw = a.nvs.tekst("cpap", "sp_cofw", "");
    int el = cw.empty() ? EL_MASKA : liczbaCalk(cw.c_str());
    if (el < 0 || el >= EL_N) el = EL_MASKA;
    p.tytul = sk::tekst("R-MAS-01.tytul");
    p.powod = sk::tekst(el == EL_MASKA ? "R-MAS-01.maska" : "R-MAS-01.filtr");
    if (cofniecieWymianyB16()) p.stopka = {"◀ 2 s cofnij"};
    rysujPotwierdzenie(plotno(), p);
  }
  // R-MAS-11 (jak R-NOR-26): historia do normy bez M-07 nocy sprzed nowego modelu maski; po 120 dniach [PRÓG] okno
  // normy (90 dni) i tak ich nie sięga, więc bez kopii.
  // `sp_model` z przyszłości (zegar) ignorowany. Kopia raz na zmianę historii, nie w każdym stany().
  std::vector<Noc> histNormyB16_;
  std::string kluczHistNormyB16_;
  const std::vector<Noc>& histNormyB16(const std::string& dzis) {
    std::string m = a.nvs.tekst("cpap", "sp_model", "");
    if (!dataOk(m) || !dataOk(dzis) || m > dzis || dniMiedzy(m, dzis) > 120) return hist;
    double s = 0;
    for (auto& n : hist) s += (std::isnan(n.wp95) ? 0 : n.wp95) + (std::isnan(n.wmed) ? 0 : n.wmed) * 1e-3;
    char b[64];
    snprintf(b, sizeof b, "%u;%.6f", (unsigned)hist.size(), s);
    std::string kl = m + "|" + b + "|" + (hist.empty() ? "" : hist.back().data);
    if (kl != kluczHistNormyB16_) {
      histNormyB16_ = hist;
      for (auto& n : histNormyB16_)
        if (n.data < m) n.wmed = n.wp95 = NAN;
      kluczHistNormyB16_ = kl;
    }
    return histNormyB16_;
  }
  // Czy linia jednorazowa `linia:{id}` czeka na pokazanie: nie była pokazana od `od` (pierwsza noc zdarzenia) albo stoi
  // dziś (przerysowanie tego samego dnia). Pokazanie liczy pamięć reguł, czyli rzeczywiste wyświetlenie (R-PIE-08).
  bool liniaJednorazowaCzeka(const char* id, const std::string& od, const std::string& dzis) {
    auto i = b5.pamiec.find(sprawy::kluczPokazania(std::string("linia:") + id));
    std::string ost = i == b5.pamiec.end() ? "" : i->second.ostatnio;
    return ost.empty() || ost < od || ost == dzis;
  }
  std::vector<const Noc*> nocePosortowaneB21(const std::string& dzis) const {
    std::vector<const Noc*> v;
    for (auto& n : hist)
      if (dataOk(n.data) && n.data <= dzis) v.push_back(&n);
    std::stable_sort(v.begin(), v.end(), [](const Noc* x, const Noc* y) { return x->data < y->data; });
    return v;
  }
  // R-NOR-14: `Nowe ustawienia aparatu. Uczę się normy.` jednorazowo dla zmiany ustawień (R-NOR-07) między nocami ważnymi
  // w trybie obsługiwanym; brak odczytu nie jest zmianą (R-NOR-15), noc spoza wydania nie liczy się (R-DANE-29). Bez
  // wartości we wszystkich trybach (ADR-0002, spec R-NOR-14 i 4.11). Linia czeka do pokazania, najdłużej 14 dni od pierwszej nocy
  // z nowymi ustawieniami [PRÓG: rozruch metryki trwa 14 nocy], żeby stara zmiana w historii nie wracała po aktualizacji.
  void liniaZmianyUstawienB21(const std::string& dzis) {
    std::string ost, zmiana;
    for (const Noc* n : nocePosortowaneB21(dzis)) {
      if (!n->wazna || n->czesciowa || n->ustawienia.empty()) continue;
      if (!ost.empty() && n->ustawienia != ost) zmiana = n->data;
      ost = n->ustawienia;
    }
    if (zmiana.empty() || !dataOk(dzis) || dniMiedzy(zmiana, dzis) > 14 /* R-NOR-14.oczekiwanie [PRÓG] */) return;
    if (!liniaJednorazowaCzeka("R-NOR-14", zmiana, dzis)) return;
    b5.linieE.push_back({"R-NOR-14", "norma", sk::tekst("R-NOR-14.linia")});
  }
  // R-DANE-29: raz na serię nocy w tym samym trybie spoza wydania albo nieznanym (kolejna dopiero po nocy w trybie
  // obsługiwanym): `Tryb aparatu {tryb}: ta wersja go nie ocenia.` albo `Nie znam trybu aparatu: tej nocy nie oceniam.`
  // Noc z karty bez trybu przejmuje tryb poprzedniej (jak trybBiezacy()), więc serii nie zaczyna.
  void liniaTrybuB21(const std::string& dzis) {
    std::string tryb, start;
    bool seria = false;
    for (const Noc* n : nocePosortowaneB21(dzis)) {
      std::string t = trybNocy(*n);
      if (t.empty() && n->zrodlo == 'K') continue;
      bool obcy = !trybObslugiwany(t);
      if (!obcy) { seria = false; continue; }
      if (!seria || t != tryb) { seria = true; tryb = t; start = n->data; }
    }
    if (!seria || !liniaJednorazowaCzeka("R-DANE-29", start, dzis)) return;
    std::string tekst = tryb.empty() ? "" : sk::tekst("R-DANE-29.linia_tryb", {{"tryb", tryb}});
    if (tekst.empty() || tekst.size() > 60) tekst = sk::tekst("R-DANE-29.linia_nieznany");  // tryb nieznany albo za długi
    b5.linieE.push_back({"R-DANE-29", "dane", tekst});
  }
  // R-MAS-11: linia `Nowy model maski. Uczę się normy.` jednorazowo od następnego ranka (grupa „norma”, 7.5); pokazanie
  // liczy pamięć reguł (`pokaz~linia:R-MAS-11`, R-PIE-08), więc linia trwa do końca dnia pierwszego pokazania
  void liniaNowegoModeluB16(const std::string& dzis) {
    std::string m = a.nvs.tekst("cpap", "sp_model", "");
    if (!dataOk(m) || !dataOk(dzis) || dzis <= m) return;
    auto i = b5.pamiec.find(sprawy::kluczPokazania("linia:R-MAS-11"));
    std::string ost = i == b5.pamiec.end() ? "" : i->second.ostatnio;
    if (!ost.empty() && ost > m && ost != dzis) return;
    b5.linieE.push_back({"R-MAS-11", "norma", sk::tekst("R-MAS-11.linia")});
  }
  sprzet::WynikPytaniaPoduszki pytaniePoduszkiB16(const std::string& dzis) {
    sprzet::Elementy el = sprzetB16();
    Norma nm = stany(dzis).norma;
    sprzet::WejscieSprzetu ws;
    ws.dzis = dzis;
    ws.hist = &hist;
    ws.norma = &nm;
    ws.el = &el;
    std::vector<std::string> pyt;
    for (auto& x : odpowiedziB16(sprawy::Pytanie::PODUSZKA, false))
      if (x != dzis) pyt.push_back(x);
    std::vector<std::string> inf;
    for (auto& x : infekcja::noceOkresow(okresyB16(dzis), dzis)) inf.push_back(x);
    return sprzet::ocenPytaniePoduszki(ws, pyt, inf, "");
  }
  // Pytania jako ekran dnia (po pytaniu R0 z kolejki): S-K-30 (R5a) i `Wymieniona poduszka?` (R8).
  void pytanieB16(const std::string& dzis, const sprawy::SprawaL* ekranL) {
    if (b5.pytanie.jest || !zegar()) return;
    int m = minutaDoby();
    if (m < 5 * 60 || m >= b5.termin) return;
    if (ekranL && (ekranL->pilnosc == reguly::Pilnosc::A || ekranL->pilnosc == reguly::Pilnosc::B)) return;  // R2, R3
    if (kolejka::niewyciszonyA(b5.sprawy, dzis)) return;  // R-PIE-06, przy niewyciszonym A bez pytań
    DaneDnia d0;
    sk::Wejscie w = wejscieB16(dzis, d0);
    std::vector<sk::Tresc> v = sk::rano(w);
    for (auto& t : v) {
      if (t.doLogu) continue;
      if (t.id == "S-K-30" && !sprawy::jestOdpowiedz(odpowiedzi, dzis, sprawy::Pytanie::INFEKCJA)) {
        b5.pytanie.jest = true;
        b5.pytanie.pytanie = sprawy::Pytanie::INFEKCJA;
        b5.pytanie.data = dzis;
        return;
      }
      if (sk::ranga(t.grupa) * 10 + t.kolejnosc < sk::ranga("R8") * 10 + 2) return;  // ekran wyżej niż pytanie R8
      break;
    }
    if (ekranL || sprawy::jestOdpowiedz(odpowiedzi, dzis, sprawy::Pytanie::PODUSZKA)) return;
    if (pytaniePoduszkiB16(dzis).jest) {
      b5.pytanie.jest = true;
      b5.pytanie.pytanie = sprawy::Pytanie::PODUSZKA;
      b5.pytanie.data = dzis;
    }
  }
  bool rysujPytanieB16(const DaneDnia& d, const std::string& dzis) {
    if (b5.pytanie.pytanie == sprawy::Pytanie::INFEKCJA) {
      sk::Wejscie w = wejscieB16(dzis, d);
      for (auto& t : sk::rano(w))
        if (t.id == "S-K-30") {
          // R-CZY-25: na pytaniu tak/nie ↻ znaczy „pomiń”, a ↻ 2 s nic, więc linia zaproszenia tu nie stoi
          // (inne linie zostają). Zwykle tu nie dochodzi, bo R-CZY-15 nie tworzy zaproszenia w dniu S-K-30; zostaje
          // zabezpieczeniem na wpis czynników sprzed chwili, w której S-K-30 stał się kandydatem (np. noc
          // zaimportowana później tego ranka). Wtedy linia gaśnie bez następczyni: kolejka linii jest już
          // policzona na ten dzień, a jej ponowne liczenie tylko dla tego rzadkiego przypadku nie jest warte ryzyka.
          bool zapr = d.linia == TEKST_ZAPROSZENIA;
          t.t.linia = zapr ? "" : d.linia;
          t.t.liniaA = !zapr && d.liniaA;
          t.t.liniaOstrzezenie = !zapr && d.liniaOstrzezenie;
          sk::rysuj(plotno(), Naglowek{naglowekDniaLewo(d), naglowekDniaPrawo(d)}, t, "", "");
          return true;
        }
      return false;
    }
    if (b5.pytanie.pytanie == sprawy::Pytanie::PODUSZKA) {
      sprzet::WynikPytaniaPoduszki q = pytaniePoduszkiB16(dzis);
      DanePytaniaTN p = trescPytaniaTN(sprawy::Pytanie::PODUSZKA, d, b5.termin, 0);
      p.opis = sk::tekst("R-MAS-12.opis", {{"y", calkowita(q.przed)}, {"x", calkowita(q.po)}, {"data", ddmm(q.odNocy)}});
      rysujPytanieTN(plotno(), p);
      return true;
    }
    return false;
  }
  // Punkt wejścia z rysujDzien po kolejce linii true = ekran narysowany (slot dzienny, S-K-01/S-K-02 wieczorem,
  // S-K-13 rano). `wpisNocny`: slot nocny bez S9 z ekranem z dziennika (ten sam ekran co wieczorem).
  bool rysujKontekstB9(const std::string& typ, DaneDnia& dd, bool wpisNocny = false) {
    std::string dzis = dzisiaj();
    int m = minutaDoby();
    if (dzis.empty() || !zegar()) return false;
    uint32_t ep = teraz();
    // wieczór (prototyp: okno stałe 21:00–23:30, R-KON-17) i ekrany CO2
    // okno slotu wieczornego z R-KON-09 i R-KON-17 (src/slot_wieczorny.h), ocena przy każdym wybudzeniu w oknie
    // (plan: co 30 min, R-KON-01)
    bool wieczor = slot_wieczorny::wOknie(slot_wieczorny::oknoZHistorii(hist, dzis), m);
    if (wpisNocny || (wieczor && !zostajeRano(typ, false))) {
      if (!wpisNocny && !co2B17()) return false;  // egzemplarz bez ekranów CO2 (5.8.8): wieczór bez S-K-01, S-K-02
      std::string dzien = wpisNocny ? a.nvs.tekst("cpap", "b9_wiecz", "").substr(0, 10) : dzis;
      if (!dataOk(dzien)) return false;
      WejscieWieczoru w;
      w.dzis = dzien;
      Co2Nocy n = co2NocyB9(dzien);
      w.noc = &n;
      w.co2Teraz = wpisNocny ? -1 : co2Teraz(r.oto, ep);
      int pm = pm25Teraz(r.oto, ep);
      w.pm25ZaOknem = pm >= 0 ? (float)pm : NAN;
      w.tempSypialni = temp;
      w.dziennik = &dziennik;
      w.prosbaDzis = prosbaDzisB9(dzien);
      w.wieczorowZKrokiem7 = wieczoryZKrokiemB9(dzien);
      std::string ost = a.nvs.tekst("cpap", "b9_wost", "");  // `data;z;wartość` ostatniego pokazania (7.4, 25%)
      if (ost.size() > 13) {
        w.poprzedniZOdczytu = ost[11] == '1';
        w.poprzedniaWartosc = liczbaCalk(ost.c_str() + 13);
      }
      // r01: wybór dnia zapamiętany przy pierwszym pokazaniu (`data;ekran;z;wartość`), bez przeliczania
      WynikWieczoru wy;
      std::string zap = a.nvs.tekst("cpap", "b9_wiecz", "");
      bool zPamieci = zap.size() > 15 && zap.compare(0, 10, dzien) == 0;
      if (zPamieci) {
        wy.ekran = zap[11] == '2' ? EkranWieczoru::SK02 : EkranWieczoru::SK01;
        wy.zOdczytu = zap[13] == '1';
        wy.wartosc = liczbaCalk(zap.c_str() + 15);
      } else if (!wpisNocny) {
        wy = ocenWieczor(w);
        if (wy.ekran == EkranWieczoru::BRAK) return false;
        if (wy.doLogu) { log("tryb próbny: %s w logu (wartość %d)", wy.ekran == EkranWieczoru::SK02 ? "S-K-02" : "S-K-01", wy.wartosc); return false; }
        std::string z = dzien + ";" + (wy.ekran == EkranWieczoru::SK02 ? "2" : "1") + ";" + (wy.zOdczytu ? "1" : "0") + ";" +
                        std::to_string(wy.wartosc);
        a.nvs.ustawTekst("cpap", "b9_wiecz", z);
        a.nvs.ustawTekst("cpap", "b9_wost", dzien + ";" + (wy.zOdczytu ? "1" : "0") + ";" + std::to_string(wy.wartosc));
      } else {
        return false;
      }
      bool sk02 = wy.ekran == EkranWieczoru::SK02;
      TrescKontekstu trk = sk02 ? trescSK02(w) : trescSK01(w, wy);
      trk.t.linia = dd.linia;
      trk.t.liniaA = dd.liniaA;
      trk.t.liniaOstrzezenie = dd.liniaOstrzezenie;
      const char* t = sk02 ? "SK02" : "SK01";
      Naglowek nag{naglowekSlotu(dzien) + " · wieczór",
                   w.co2Teraz >= 0 ? "sypialnia " + godzina(r.oto.tsSypialni) + " · CO2 " + std::to_string(w.co2Teraz) : ""};
      const WpisHumoru* wp = nullptr;
      for (auto& x : dziennik)
        if (x.data == dzien && x.typ == t) wp = &x;
      Wybor wb;
      if (wp) { wb.tryb = wp->tryb; wb.scena = wp->scena; wb.zdanie = wp->zdanie; }
      else {
        EkranDnia ed = ekranKarty(t, "co2", dzien, dziennik, trk.t.krok);  // 5.7.3, R-HUM-07
        TrescEkranu tt = trk.t;
        tt.tytul += ".";
        KontekstHumoru kh;
        kh.dzis = dzien;
        kh.humor = humorB32();
        kh.liniaUwagi = !dd.linia.empty();  // R-HUM-17: z linią uwagi ekran danych
        kh.zPropozycjami = Z_PROPOZYCJAMI;
        kh.sceny = sceny();
        wb = wybierzTryb(ed, kartaMiesci(tt), dziennik, kh);
        dopiszDziennik({dzien, t, ed.klucz, wb.tryb, wb.scena, wb.zdanie});
        log("ekran wieczorny %s: %s", t, wb.tryb == 'K' ? "karta" : wb.dlaczego);
      }
      if (wb.tryb == 'K' && dd.linia.empty()) {
        TrescEkranu tt = trk.t;
        tt.tytul += ".";
        tt.etykieta = wersaliki(tt.etykieta);
        tt.stopkaPrawo = stopkaCzujnika(tempStopki(), wilgStopki());
        tt.stopkaLewo = dd.pogoda;  // jak ekran danych (rysujEkranKontekstu), bez zdania budzika od 10.11.2026
        dopasujStopke(tt.stopkaLewo, tt.stopkaPrawo);  // R-EKR-10: ustępują opis pogody, miasto, dopisek CO2
        rysujKarte(plotno(), nag, tt, wb.scena);
      } else {
        rysujEkranKontekstu(plotno(), nag, trk, dd.pogoda, dd.przyLozku);
      }
      dzienOdswiezalny = false;
      r.dzienTryb = 0;
      return true;
    }
    // slot dzienny (R-KON-41: od startu do początku slotu wieczornego; okno z R-KON-09, przy oknie od 19:00 do 19:00)
    bool slot = sekundaDzis(r.slotDziennyEp) >= 0 && !k.bezSlotuDziennego &&
                m < koniecSlotuDziennego(slot_wieczorny::oknoZHistorii(hist, dzis));
    if (!slot) {
      // rano przed slotem dziennym: S-K-13 (R8) przed dowodem spokoju i rozruchem
      if (m >= 300 && rysujR6R7B15(typ, dd)) return true;  // S7 (R6), S8 (R7) przed S-K-13 (R8)
      if (m >= 300 && grupaRano(typ) == 9) {
        WejscieRanka wr;
        wr.dzis = dzis;
        Co2Nocy n = co2NocyB9(dzis);
        wr.noc = &n;
        wr.poprzedniWieczorSK01 = false;
        for (auto& x : dziennik)
          if (x.typ == "SK01" && x.data == dodajDni(dzis, -1)) wr.poprzedniWieczorSK01 = true;
        wr.pozaDomem = nocePozaDomem(czynniki).count(dzis) > 0;
        wr.dziennik = &dziennik;
        WynikRanka wy = co2B17() ? ocenRano(wr) : WynikRanka{};  // bez ekranów CO2 bez S-K-13 (5.8.8)
        if (!wy.sk13 && !wpisDzis("SK13", dzis)) return rysujOdkrycieB15(typ, dd);  // odkrycie (R8) po S-K-13
        if (wy.doLogu) { log("tryb próbny: S-K-13 w logu"); return false; }
        if (!wpisDzis("SK13", dzis)) dopiszDziennik({dzis, "SK13", std::to_string(n.minutPonad), 'D', "", ""});
        Naglowek nag{naglowekDniaLewo(dd), naglowekDniaPrawo(dd)};
        TrescKontekstu trk = trescSK13(n);
        trk.t.linia = dd.linia;
        trk.t.liniaA = dd.liniaA;
        trk.t.liniaOstrzezenie = dd.liniaOstrzezenie;
        rysujEkranKontekstu(plotno(), nag, trk, dd.pogoda, dd.przyLozku);
        dzienOdswiezalny = false;
        r.dzienTryb = 0;
        return true;
      }
      return m >= 300 && rysujOdkrycieB15(typ, dd);
    }
    if (zostajeRano(typ, false)) return false;  // D0
    const Noc* noc = nocZDnia(hist, dzis);
    bool ubiegla = noc != nullptr;
    if (!noc && !hist.empty()) noc = &hist.back();
    const Prognoza& pr = prognozaB9();
    bool wazna = prognozaWazna(pr, ep);
    const UstawieniaDnia& us = ustawieniaB9();
    WejscieSlotu we;
    we.dzis = dzis;
    we.minuta = m;
    we.prognoza = wazna ? &pr : nullptr;
    we.przyLozku = temp;  // wybór i wizualizacja ze świeżego odczytu; stopka niżej z klatki (stopkaZPamieci)
    float przyStopki = tempStopki();
    we.co2 = co2B17() ? co2Teraz(r.oto, ep) : -1;  // wariant CO2 S-K-33 tylko z ekranami CO2 (5.8.8)
    we.pylki = us.pylki;
    we.sprzet = &us.sprzet;
    we.ostatnieSK35 = a.nvs.tekst("cpap", "b9_sk35", "");
    we.wspolrzedne = miastoB9();
    we.lat = pogodaLatB32();
    we.lon = pogodaLonB32();
    we.linia = dd.linia;
    we.liniaA = dd.liniaA;
    we.liniaOstrzezenie = dd.liniaOstrzezenie;
    we.liniaUwagi = !dd.linia.empty();
    { int so = sekundaDzis(r.slotDziennyEp); we.slotOd = so >= 0 ? so / 60 : -1; }  // blok obrazka od startu slotu
    we.drzemka = drzemkaDzisB9(dzis);
    // R-KON-56: drzemka trwa → ten sam ekran co w ostatniej ocenie (minuta, czujnik i CO2 z niej), więc klatka bez zmian
    std::string zam = a.nvs.tekst("cpap", "b9_zam", "");
    if (drzemkaTrwaB9() && zam.size() > 11 && zam.compare(0, 10, dzis) == 0) {
      std::vector<std::string> f;
      size_t p0 = 0;
      for (;;) {
        size_t q = zam.find(';', p0);
        f.push_back(zam.substr(p0, q == std::string::npos ? std::string::npos : q - p0));
        if (q == std::string::npos) break;
        p0 = q + 1;
      }
      if (f.size() == 4) {
        we.minuta = liczbaCalk(f[1].c_str());
        we.przyLozku = f[2].empty() ? NAN : (float)liczbaRzecz(f[2].c_str());
        przyStopki = we.przyLozku;
        we.co2 = co2B17() ? liczbaCalk(f[3].c_str()) : -1;  // `co2=nie` w trakcie drzemki bez wariantu CO2
        log("drzemka trwa (rop 1): ekran bez zmian (R-KON-56)");
      }
    } else {
      char b[64];
      snprintf(b, sizeof b, "%s;%d;%s;%d", dzis.c_str(), m, std::isnan(temp) ? "" : calkowita(temp).c_str(), we.co2);
      a.nvs.ustawTekst("cpap", "b9_zam", b);
    }
    // obrazek dnia (R-KON-59…R-KON-62): katalog /obrazki/ w pamięci panelu, dziennik w magazynie (format `obrazki`)
    std::vector<std::string> pliki;
    std::vector<WpisObrazka> dzo;
    std::vector<PozycjaManifestu> man;
    WyborObrazka wo;
    if (fsOk() && a.pliki.listuj(KATALOG_OBRAZKOW, pliki)) {
      std::string t;
      bool jestMan = a.pliki.czytaj(MANIFEST_OBRAZKOW, t) && parsujManifest(t, man);
      for (auto& l : wczytaj("obrazki")) {
        WpisObrazka x;
        if (parsujWpisObrazka(l, x)) dzo.push_back(x);
      }
      KontekstObrazka ko;
      ko.dzis = dzis;
      ko.godzina = m / 60;
      ko.prognoza = we.prognoza;
      auto czytelny = [&](const std::string& pl) {
        std::string dane, pw;
        ObrazPbm o;
        return a.pliki.czytaj(pl.c_str(), dane) && parsujPbm(dane, o, pw);
      };
      wo = wybierzObrazek(pliki, jestMan ? &man : nullptr, dzo, ko, czytelny);
      for (auto& l : wo.log) log("%s", l.c_str());
    }
    we.obrazekJest = !wo.plik.empty();
    WyborSlotu w = wybierzSlot(we);
    if (w.ekran == EkranSlotu::BRAK) return false;  // W4: ekran z rana (R-KON-54)
    float zaOknem = NAN;
    if (we.prognoza) {
      const GodzinaPrognozy* g = pr.godzina(dzis, m / 60);
      if (g) zaOknem = g->temp;
    }
    if (std::isnan(zaOknem) && r.oto.pogoda && swiezy(r.oto.tsPogody, ep, OTO_SWIEZOSC_POGODY_S)) zaOknem = r.oto.temp;
    // ocena o pełnej godzinie: temperatura za oknem z klatki, tak jak przy łóżku (R-KON-42)
    if (stopkaZPamieci && r.klatkaOd && !std::isnan(r.stopkaZaOknem)) zaOknem = r.stopkaZaOknem;
    zaOknemKlatki = zaOknem;
    StopkaDzienna st = stopkaDzienna(noc, ubiegla, we.drzemka.minut, zaOknem, przyStopki);
    if (w.ekran == EkranSlotu::SK38) {
      std::string dane, pw;
      ObrazPbm o;
      if (!a.pliki.czytaj(wo.plik.c_str(), dane) || !parsujPbm(dane, o, pw)) return false;
      if (wo.nowy) dopiszFormat("obrazki", zapiszWpisObrazka({dzis, wo.plik.substr(wo.plik.rfind('/') + 1), wo.pula}));
      ustawIkoneAktualizacji(false);  // R-EKR-61: bez ikony na obrazku dnia
      rysujObrazekDnia(plotno(), o, st);
      log("slot dzienny: S-K-38 %s (pula %c)", wo.plik.c_str(), wo.pula);
    } else {
      TekstySlotu t = tekstySlotu(w, we);
      Naglowek nag{naglowekSlotu(dzis), we.prognoza && w.ekran != EkranSlotu::SK35 && w.ekran != EkranSlotu::SK37
                                            ? "prognoza " + pr.godzinaPobrania : ""};
      if (w.ekran == EkranSlotu::SK39 && we.drzemka.od)
        nag.prawo = "drzemka " + godzina(we.drzemka.od) + "–" + godzina(we.drzemka.do_);
      if (w.ekran == EkranSlotu::SK35 && we.ostatnieSK35 != dzis) a.nvs.ustawTekst("cpap", "b9_sk35", dzis);
      if (w.ekran == EkranSlotu::SK35 && w.wymiana.element >= 0 && w.wymiana.element < EL_N) {
        // R-MAS-18: ◀ 2 s zapisuje „wymienione dziś” elementu z ekranu (jak S-K-31: `b16_sk31` po rysujDzien()),
        // stopka jak na S-K-31 (ta sama akcja i ten sam zapis): `◀ 2 s: wymienione dziś` i `▶ kokpit` zamiast liczb nocy
        // i odczytu czujnika (5.6.1 pkt 6, R-PRZ-04)
        b16.sk31 = (uint32_t)w.wymiana.element + 1;
        st.lewo = sprzet::STOPKA_WYMIANY_LEWO;
        st.prawo = sprzet::STOPKA_WYMIANY_PRAWO;
        log("slot dzienny: S-K-35 %s, ◀ 2 s zapisuje wymianę (R-MAS-18)", NAZWY_ELEMENTU[w.wymiana.element]);
      }
      rysujSlotDzienny(plotno(), nag, w, t, we, st);
      log("slot dzienny: %s", idEkranuSlotu(w.ekran));
    }
    dzienOdswiezalny = false;
    r.dzienTryb = 0;
    return true;
  }

  // R-CZY-20: szczegół O-12 w K1 tylko z ekranu dnia, który go pokazał; stan w NVS `cpap`/`k1_odk`
  // (`data\ttytuł\tpowód`, "" = brak), bo ▶ zwykle budzi panel w nowym wybudzeniu (ADR-0022: zapis tylko przy zmianie)
  void rysujDzien() {
    // element S-K-31 na ekranie dnia (◀ 2 s, R-MAS-01) zapisany po każdym rysowaniu, także po wcześniejszym wyjściu
    struct ZapisSk31 {
      Przebieg* p;
      ~ZapisSk31() { p->a.nvs.ustawLiczbeU("cpap", "b16_sk31", p->b16.sk31); }
    } zapisSk31{this};
    b16.sk31 = 0;
    k1OdkrycieB15 = OdkrycieK1B15();
    ekranDniaAB = false;
    {
      // R-EKR-61: ikona aktualizacji w stopce ekranu dnia (bez wpływu na wybór ekranu); wyłączana w S9, obrazku dnia
      // i na ekranie L A/B, kładziona po narysowaniu (tło stopki z bufora: stopka w negatywie ma ikonę odwróconą)
      IkonaStopki ik(ikonaAktualizacjiTeraz());
      rysujDzienB15Wewn();
      zapiszSpraweAB(ekranDniaAB);  // pilność ekranu stojącego na panelu do następnego wybudzenia
      if (ikonaAktualizacjiWlaczona()) {
        const uint8_t* b = a.wyswietlacz.bufor();
        bool tloCzarne = b && (b[(size_t)476 * 100 + 795 / 8] & (0x80 >> (795 & 7)));
        ikonaAktualizacji(plotno(), tloCzarne);
        log("ekran dnia: ikona aktualizacji (jest wersja %s)", akt.znana.c_str());
      }
    }
    if (drugiEkranTeraz && r.slotOdlozony && zegar()) {  // odłożony start slotu po drugim ekranie
      r.slotDziennyEp = teraz();
      r.slotOdlozony = 0;
      log("slot dzienny: start odłożony do drugiego ekranu poranka (R-POR-04)");
    }
    drugiEkranTeraz = false;
    std::string z = k1OdkrycieB15.dzien.empty()
                        ? std::string()
                        : k1OdkrycieB15.dzien + "\t" + k1OdkrycieB15.tytul + "\t" + k1OdkrycieB15.powod;
    if (a.nvs.tekst("cpap", "k1_odk", "") != z) a.nvs.ustawTekst("cpap", "k1_odk", z);
  }
  void wczytajK1OdkrycieB15() {
    if (!k1OdkrycieB15.dzien.empty()) return;
    std::string z = a.nvs.tekst("cpap", "k1_odk", "");
    size_t t1 = z.find('\t'), t2 = t1 == std::string::npos ? t1 : z.find('\t', t1 + 1);
    if (t2 == std::string::npos) return;
    k1OdkrycieB15 = {z.substr(0, t1), z.substr(t1 + 1, t2 - t1 - 1), z.substr(t2 + 1)};
  }
  void rysujDzienB15Wewn() {
    // R-EKR-60: przerysowanie co 2 h tylko ekranu dnia ze stopką standardową (R-EKR-10, karta albo ekran danych);
    // kryterium to stopka z trescDnia, nie lista wykluczeń
    dzienOdswiezalny = false;
    r.portretStrona = 0;  // ustawiają ją niżej tylko ekrany problemu z Portretem pod ▶ (R-POR-24)
    DecyzjaNocy dn = nocTeraz(true);  // slot nocny 23:30–05:00: ekranem dnia jest „Dobranoc”, gdy powstał tej nocy
    if (dn.nocny) {
      silnikDzis(dzisiaj());  // wynik gotowy na 05:00 (log), bez zmiany ekranu nocnego (R-PRZ-18)
      rysujNocny(dn);
      r.dzienTryb = 0;
      return;
    }
    WejscieDnia we = wejscieDnia();
    std::string dzis = we.dzis;
    // przegląd tygodnia rano niezależnie od ekranu dnia (niedziela zajęta innym ekranem nie przesuwa go
    // poza poniedziałek); pokazanie odkrycia dalej tylko w spokojnym ranku (grupa 9, rysujOdkrycieB15())
    if (!dzis.empty() && minutaDoby() >= 5 * 60 && silnikDzis(dzis)) przegladOdkrycB15(dzis);
    if (dzis.empty()) {  // bez zegara: bez historii i bez dziennika ekranów
      DaneDnia d = daneDnia(dzis);
      Naglowek n{naglowekDniaLewo(d), naglowekDniaPrawo(d)};
      TrescEkranu t = trescDnia(brakDanych(we) ? "S5" : "S2A", "", d);
      rysujEkranDanych(plotno(), n, t);
      dzienOdswiezalny = t.stopkaStandardowa;
      r.dzienTryb = 0;
      return;
    }
    // E-05 krytyczne → S5 (R1, po E-01 i E-03), poza dniem pełnego ekranu wzorca A (R-PIE-07): we.e05Krytyczne
    // z wejscieDnia() (e05KrytyczneTeraz()), w slocie nocnym też (decyzjaDnia(): S5 baterii zostaje do 05:00)
    DecyzjaDnia dd = decyzjaDnia(we, dziennik);
    EkranDnia e = dd.e;
    DaneDnia d = daneDnia(dd.dataEkranu);
    if (e.typ == "S5" && e.klucz == "E-05") tekstyE05B5(d);
    Naglowek n{naglowekDniaLewo(d), naglowekDniaPrawo(d)};
    if (!dd.nowyWybor) {  // slot nocny (U1): ekran z ostatniego wpisu dziennika
      if (dd.wpis && (dd.wpis->typ == "SK01" || dd.wpis->typ == "SK02") && rysujKontekstB9(e.typ, d, true)) return;
      if (dd.wpis && rysujWpisB15(*dd.wpis, d)) return;  // S7, S8, S1 z dziennika
      if (dd.wpis && sk::typB16(dd.wpis->typ) && rysujNocB16(*dd.wpis, d, n)) return;  // ten sam ekran co wieczorem
      TrescEkranu t = trescDnia(e.typ == "S2D" ? "S2A" : e.typ, e.typ == "S2D" ? "" : e.klucz, d);  // przerywnik stoi za dowód spokoju
      // karta z dziennika tylko, gdy zdanie jest w dzisiejszej puli i (uspokajające) warunek uspokojenia jest spełniony;
      // inaczej ekran danych (po zmianie puli 08.11.2026 stary numer może wskazywać nic albo zdanie uspokajające)
      const std::string dzienWpisu = dd.wpis ? dd.wpis->data : dzis;
      bool karta = dd.wpis && dd.wpis->tryb == 'K' &&
                   kartaZWpisuWazna(dd.wpis->zdanie, [&] { return stany(dzienWpisu).uspokojenie; });
      if (karta && e.typ == "S2D")  // R-HUM-28: wariant tekstu z faktów nocy dnia ekranu, jak przy rysowaniu rano
        trescPrzerywnikaB17(t, dd.wpis->scena, d, warunekPrzerywnika(dd.wpis->scena, nocPrzerywnikaDnia(d.dzis)));
      if (karta) rysujKarte(plotno(), n, t, dd.wpis->scena);
      else rysujEkranDanych(plotno(), n, t);
      dzienOdswiezalny = t.stopkaStandardowa;
      r.dzienTryb = 0;
      return;
    }
    if (e.typ == "S10") {  // R-EKR-57: karta bez wpisu w dzienniku
      liniaPilnaB5(d, dzis, true);  // linia A także na S10 (5.12.2); z linią ekran danych (R-HUM-17);
      TrescEkranu t = trescDnia("S10", "", d);
      std::string z = zdanieSynchronizacji(dzis, Z_PROPOZYCJAMI);
      std::vector<std::string> sc = sceny();
      bool jest = std::find(sc.begin(), sc.end(), std::string("B05")) != sc.end();
      if (d.linia.empty() && kartaS10(humorB32(), z, jest, kartaMiesci(t))) rysujKarte(plotno(), n, t, "B05");
      else rysujEkranDanych(plotno(), n, t);
      dzienOdswiezalny = t.stopkaStandardowa;  // S10: `↻ pobierz teraz`, więc bez przerysowań
      r.dzienTryb = 0;
      log("ekran dnia S10 (synchronizacja): %s", z.empty() ? "dane" : "karta B05");
      return;
    }
    if (!dd.zapisz) {  // wybór tymczasowy przed nocą z dziś: ekran danych bez wpisu w dzienniku
      liniaPilnaB5(d, dzis, true);  // także linia baterii krytycznej w dniu pełnego ekranu A
      TrescEkranu t = trescDnia(e.typ, e.klucz, d);
      rysujEkranDanych(plotno(), n, t);
      dzienOdswiezalny = t.stopkaStandardowa;
      r.dzienTryb = 0;
      return;
    }
    WpisCzynnikow* zap = wpisZDnia(czynniki, dzis);
    // R-EKR-60 bez pobrania: tryb i linia uwagi z ostatniego narysowania (RTC), zmienia się tylko stopka
    bool zPamieci = przerysowanie && r.dzienTryb != 0 && e.typ != "S5";
    LiniaDnia ld;
    if (zPamieci) {
      ld.zaproszenie = r.dzienLinia == 1;
      ld.e08 = r.dzienLinia == 2;
    } else {
      ld = liniaDnia(zaproszenieTeraz(), e.typ, r.oto, teraz(), dzis, stanE08, co2B17());
    }
    bool linia = ld.zaproszenie;
    d.linia = linia ? TEKST_ZAPROSZENIA : "";
    d.liniaOstrzezenie = linia && liniaJestOstrzezeniem(GrupaLinii::PYTANIA);
    if (ld.e08) {  // E-08 (grupa „dane”, 7.5: pod zaproszeniem z grupy 2a): `co2_milczy`, raz na epizod
      std::string dOd;
      int mOd;
      dataIMinuta(r.oto.co2MilczyOd, dOd, mOd);
      d.linia = tekstE08(dOd, mOd, dzis);
      d.liniaOstrzezenie = liniaJestOstrzezeniem(GrupaLinii::DANE);
    }
    if (ld.zapiszE08) {
      a.nvs.ustawLiczbeU("cpap", "e08_od", stanE08.od);
      a.nvs.ustawTekst("cpap", "e08_d", stanE08.dzien);
      a.nvs.ustawFlage("cpap", "e08_k", stanE08.koniec);
    }
    if (awariaPamieci()) {  // R-AW-16 (T-WYM-34, R7): awaria pamięci przed pytaniem i danymi (grupa „awarie”, 7.5)
      linia = false;
      ld.e08 = false;
      d.linia = TEKST_R_AW_16;
      d.liniaOstrzezenie = liniaJestOstrzezeniem(GrupaLinii::AWARIE);
    }
    // wyniki silnika → pytanie tak/nie albo ekran wzorca L jako ekran dnia, inaczej linia z kolejki 7.5 (linie
    // spoza silnika wchodzą do kolejki w swoich grupach). Bez wyników wszystko jak dotąd.
    {
      std::string zapT = linia ? d.linia : "", e08T = ld.e08 ? d.linia : "", awT = awariaPamieci() ? d.linia : "";
      if (e.typ == "S5") {
        liniaPilnaB5(d, dzis);  // S5: tylko linia grupy 0 (przypadek 1 z 7.8), bez ekranu L i pytań
      } else if (silnikDzis(dzis, zapT, e08T, awT)) {
        if (rysujDzienB5(d, n, dzis, e)) {
          // ekran L, S3 albo problem poranny (R2–R5d): Portret jako strona pod ▶ przed K1 (R-POR-24); pytanie R0 nie
          r.portretStrona = !b5.pytanie.jest && portretRanoDostepny(dzis) && sekundaDzis(r.slotDziennyEp) < 0 ? 1 : 0;
          return;
        }
        kolejka::ustawLinie(d, b5.linie);
        if (!d.linia.empty()) log("linia uwagi: %s (%s)", d.linia.c_str(), b5.linie.front().zrodlo.c_str());
        if (!b5.linie.empty()) pokazanieB11(kolejka::kluczPokazaniaLinii(b5.linie.front()), dzis);  // R-PIE-08
        std::string zr = b5.linie.empty() ? "" : b5.linie.front().zrodlo;
        linia = zr == "zaproszenie";
        ld.e08 = zr == "E-08";
        d.stany = &b5.stany;  // „W normie” i zdania uspokajające z oceną pkt 1, 4, 7 (5.4.13)
        buzzerB5(dzis);
      }
    }
    // Portret nocy (S11, pozycja RP tabeli 7.2): pierwszy ekran poranka przed grupami R5e–R9, po R0–R5d (R-POR-02)
    if (rysujPortretB40(e, d, n, dzis)) return;
    if (rysujB16(e, d, n, 1)) return;  // tydzień T2 (S-K-18), wieczór S-K-05, rano R4…R7 (7.2)
    if (rysujKontekstB9(e.typ, d)) return;  // slot dzienny, S-K-01/S-K-02, S-K-13 (5.8.12, 5.8.3), z linią z kolejki
    if (rysujB16(e, d, n, 2)) return;  // wieczór S-K-03…S-K-09, W4, rano R8 (S-K-19…S-K-21 w poniedziałek)
    bool jestLinia = !d.linia.empty();
    TrescEkranu t = trescDnia(e.typ, e.klucz, d);
    if (e.typ == "S5") log("S5: %s / %s", t.tytul.c_str(), t.powod.c_str());  // tekst E-01/E-03 do logu doby
    PlanWyboru pw = planWyboru(dd.wpis, e.typ, jestLinia);
    Wybor w = pw.wybor;
    // R-HUM-28: fakty ostatniej nocy do wyboru tekstu przerywnika (warunkowy albo neutralny), także przy ekranie z wpisu
    // dziennika; stan AHI jak przy uspokojeniu
    const NocPrzerywnika nocP = nocPrzerywnikaDnia(dzis);
    bool warunkowaP = false;
    if (!pw.zWpisu) {
      KontekstHumoru kh;
      kh.dzis = dzis;
      kh.humor = humorB32();
      kh.liniaUwagi = jestLinia;
      kh.zPropozycjami = Z_PROPOZYCJAMI;
      kh.uspokojenie = (b5.wyniki.empty() || b5.dzis != dzis ? stany(dzis) : b5.stany).uspokojenie;  // R-EKR-32
      kh.sceny = sceny();
      // R-HUM-26: aktywny wzorzec B, także wyciszony: S2a i S2c bez K22, bez przerywnika
      kh.wzorzecB = b5.dzis == dzis && (kolejka::aktywnyB(b5.sprawy) || kolejka::aktywnyA(b5.sprawy));  // także wyciszony A
      // przerywnik S2d (5.7.6) tylko przez wybierzPrzerywnik; w wersji beta losowany z tekstami G01–G18
      // z propozycji do akceptacji, tytuł i powód z zestawu (`S2D.{scena}.*`)
      kh.liniaUwagi = jestLinia;
      // R-HUM-14: nowy przerywnik tylko w slocie porannym (okno poranne 05:00–11:00, R-DANE-02); wybrany rano zostaje
      // ekranem z rana na resztę dnia (W4), wieczorem nowego nie ma
      bool s2dDzis = false;
      for (auto& x : dziennik) s2dDzis = s2dDzis || (x.data == dzis && x.typ == "S2D" && x.tryb == 'K');
      int mRano = minutaDoby();
      bool rano = mRano >= PLAN_OKNO_OD && mRano < PLAN_OKNO_DO;
      kh.noc = nocP;
      Przerywnik prz = rano || s2dDzis ? wybierzPrzerywnik(e, dziennik, kh, ustawieniaPrzerywnikowBety()) : Przerywnik{};
      TekstPrzerywnika tp = prz.jest ? tekstPrzerywnika(prz.scena, prz.warunkowa, dzis) : TekstPrzerywnika{};
      TrescEkranu probaP = t;
      if (prz.jest && !tp.tytul.empty()) trescPrzerywnika(probaP, tp.tytul, tp.powod, d);
      if (prz.jest && !tp.tytul.empty() && kartaPrzerywnikaMiesci(probaP)) {
        warunkowaP = prz.warunkowa;
        w.tryb = 'K';
        w.scena = prz.scena;
        w.zdanie = prz.zdanie;
        // przerysowanie tego samego dnia nie dopisuje drugiego wiersza S2D (limity kart, 7.4)
        if (pw.dopisz && !s2dDzis) dopiszDziennik({dzis, "S2D", prz.klucz, 'K', prz.scena, prz.zdanie});
      } else {
        w = wybierzTryb(e, kartaMiesci(t), dziennik, kh);
        if (pw.dopisz) dopiszDziennik({dzis, e.typ, e.klucz, w.tryb, w.scena, w.zdanie});
      }
      // przerywnik w logu jako typ `S2D-{scena}` (symulator: nazwa obrazka i podpis w spis.txt)
      bool przerywnikTeraz = w.tryb == 'K' && !w.scena.empty() && w.scena[0] == 'G';
      std::string typLogu = przerywnikTeraz ? "S2D-" + w.scena + (warunkowaP ? "-warunkowa" : "-neutralna") : e.typ;
      log("ekran dnia %s (%s): %s %s %s", typLogu.c_str(), e.klucz.c_str(), w.tryb == 'K' ? "karta" : "dane",
          w.scena.c_str(), w.tryb == 'K' ? "" : w.dlaczego);
    }
    if (zPamieci) w.tryb = r.dzienTryb;
    // to samo wyświetlenie z wpisu dziennika: karta tylko ze zdaniem z dzisiejszej puli, uspokajające tylko przy
    // spełnionym warunku uspokojenia (R-EKR-32); inaczej ekran danych (zmiana puli 08.11.2026)
    if (pw.zWpisu && w.tryb == 'K' &&
        !kartaZWpisuWazna(w.zdanie, [&] { return (b5.wyniki.empty() || b5.dzis != dzis ? stany(dzis) : b5.stany).uspokojenie; }))
      w.tryb = 'D';
    // przerywnik (wpis S2D z dziś albo wybór wyżej): tytuł, powód i fakty przerywnika (trescPrzerywnikaB17)
    if (w.tryb == 'K' && !w.scena.empty() && w.scena[0] == 'G') {
      if (pw.zWpisu) warunkowaP = warunekPrzerywnika(w.scena, nocP);
      trescPrzerywnikaB17(t, w.scena, d, warunkowaP);
    }
    t.tonProblem = e.tonProblem;  // karta „problem” S-K-10, S-K-16, S-K-31 (R-HUM-24)
    if (w.tryb == 'K') rysujKarte(plotno(), n, t, w.scena);
    else rysujEkranDanych(plotno(), n, t);
    dzienOdswiezalny = t.stopkaStandardowa;
    r.dzienTryb = w.tryb == 'K' ? 'K' : 'D';
    r.dzienLinia = linia ? 1 : ld.e08 ? 2 : 0;
    if (zPamieci) log("przerysowanie R-EKR-60: tryb i linia uwagi jak w ostatnim rysowaniu, nowa stopka");
    if (oznaczycZaproszenie(linia, zap)) {  // R-PIE-08: linia liczy się dopiero, gdy była na ekranie
      zap->pokazano = true;
      zapiszCzynniki();
    }
  }

  // (R-HUM-28): fakty ostatniej nocy dla karty przerywnika z kontekstem dnia: stan AHI i uspokojenie jak przy ekranie dnia
  // (R-EKR-32), a E-23, S3 i ekran L w 7 dniach z historii nocy, pokazań ekranów (`pokaz~…`) i dziennika
  NocPrzerywnika nocPrzerywnikaDnia(const std::string& dzis) {
    const bool silnik = !b5.wyniki.empty() && b5.dzis == dzis;
    const Stany& st = silnik ? b5.stany : stany(dzis);
    NocPrzerywnika n = nocPrzerywnika(hist, dzis, st.ahi.aktywny);
    // brak otwartej sprawy (Stany::bezSpraw: wzorce L, zdarzenia E, stan AHI i EPAP, linie „zdrowie”); „nie oceniam”
    // z braku danych i punkty, których panel nie liczy (5.4.13 pkt 3 w reszcie, pkt 5), nie blokują karty (R-HUM-29), bo
    // inaczej karta z warunkiem nigdy by nie weszła. Bez wyników silnika wzorców L nie znamy, więc karta z warunkiem odpada
    n.uspokojenie = silnik && st.bezSpraw;
    if (n.uspokojenie && n.jest) {  // 5.4.13 pkt 6: PS ostatniej nocy poza pasmem normy (ramka w K2) też zdejmuje karty pochwalne
      DaneK2 k2 = budujK2(hist, dzis, st.norma, czynniki, &st, "");
      for (int i = 0; i < k2.wierszy; i++)
        if (std::string(k2.w[i].etykieta) == "Wsparcie PS" && k2.w[i].k[6].poza) n.uspokojenie = false;
    }
    const std::string od = dodajDni(dzis, -6);
    for (auto& p : b5.pamiec)
      if (sprawy::kluczPomocniczy(p.first) && p.first.compare(0, 12, "pokaz~linia:") != 0 && p.second.ostatnio >= od && p.second.ostatnio <= dzis)
        n.problem7 = true;
    for (auto& x : dziennik)
      if (x.data >= od && x.data <= dzis && (x.typ == "S3" || x.typ == "L" || x.klucz.compare(0, 4, "E-23") == 0)) n.problem7 = true;
    return n;
  }

  // tytuł i powód przerywnika (5.7.6), bez kroku (R-HUM-21); etykieta okazji albo `Na dzień dobry`; fakty
  // tylko ostatniej nocy: M-01 i AHI (5.7.6)
  void trescPrzerywnikaB17(TrescEkranu& t, const std::string& scena, const DaneDnia& d, bool warunkowa) {
    TekstPrzerywnika tp = tekstPrzerywnika(scena, warunkowa, d.dzis);
    if (tp.tytul.empty()) return;
    trescPrzerywnika(t, tp.tytul, tp.powod, d);
  }

  // ---------- S7, S8, odkrycia (R6, R7, R8 rano przed slotem dziennym), czynniki po przerwie AirBridge ----------
  // (4.11, fail-path R-DANE-29): tryb bieżący = tryb ostatniej nocy w trybie obsługiwanym (trybBiezacy() z kokpitu,
  // ten sam co wiersze K1 i K2), a nie pole `ustawienia` ostatniej nocy, które przy `settings.available = false` bywa puste.
  reguly::Tryb trybHist() const {
    if (hist.empty()) return reguly::Tryb::NIEZNANY;
    std::string koniec = hist.front().data;
    for (auto& n : hist)
      if (n.data > koniec) koniec = n.data;
    return trybBiezacy(hist, koniec);
  }
  reguly::Tryb trybB15() const { return trybHist(); }
  // R-CZY-22: pytanie o okres „spanie poza domem” rano do terminu (R-PIE-13), gdy nie ma innego pytania tak/nie;
  // w dniu pytania „gorzej” albo o przerwę czeka do jutra, przy niewyciszonym wzorcu A go nie ma (R-PIE-06).
  // odpowiedź „nie” na R-CZY-22 i zdjęte wpisy z NVS (`cpap`/`wyj_cof`: `data|wpis|wpis…`), gdy ostatnią
  // zapisaną odpowiedzią jest ta „nie”. Okno 60 s pilnuje nawigacja (ekran „Zapisano”, cofniecieB5()). Pozostałe
  // pytania tak/nie mają tę samą lukę po uśpieniu (b5.ostatniaOdp tylko w wybudzeniu): poza zakresem.
  void wczytajCofniecieWyjazduB15() {
    std::string z = a.nvs.tekst("cpap", "wyj_cof", "");
    if (z.empty()) return;
    std::vector<std::string> cz;
    for (size_t i = 0, j; i <= z.size(); i = j + 1) {
      j = z.find('|', i);
      if (j == std::string::npos) j = z.size();
      cz.push_back(z.substr(i, j - i));
    }
    bool jest = false;
    for (auto& x : odpowiedzi) jest = jest || (x.data == cz[0] && x.pytanie == sprawy::Pytanie::WYJAZD && x.odp == 'n');
    if (!jest) return;
    b5.ostatniaOdp = sprawy::Odpowiedz{cz[0], sprawy::Pytanie::WYJAZD, 'n'};
    b5.zdjeteWyjazd.clear();
    for (size_t i = 1; i < cz.size(); i++) {
      WpisCzynnikow w;
      if (parsujWpis(cz[i], w)) b5.zdjeteWyjazd.push_back(w);
    }
  }
  void pytanieWyjazduB15(const std::string& dzis, const kolejka::WejsciePytan& wp) {
    if (b5.pytanie.jest || wp.minuta >= wp.terminRano || wp.minuta < 5 * 60) return;
    if (kolejka::niewyciszonyA(b5.sprawy, dzis)) return;
    std::set<std::string> zapytane;
    for (auto& o : odpowiedzi) {
      if (o.pytanie == sprawy::Pytanie::WYJAZD) zapytane.insert(o.data);
      if ((o.pytanie == sprawy::Pytanie::GORZEJ || o.pytanie == sprawy::Pytanie::PRZERWA) && o.data == dzis) return;
    }
    std::vector<OkresWyjazdu> ok = okresyWyjazdu(czynniki);
    const OkresWyjazdu* o = okresDoPytania(ok, zapytane, dzis);
    if (!o) return;
    b5.pytanie.jest = true;
    b5.pytanie.pytanie = sprawy::Pytanie::WYJAZD;
    b5.pytanie.data = o->od;
    b5.pytanie.przerwaNocy = o->nocy;
  }
  // R-CZY-19: przerwa od ostatniego pełnego odczytu (status i lista nocy, NVS `cpap`/`ab_pelny`), nie od
  // samego statusu; liczy się tylko, gdy panel w tej przerwie próbował i AirBridge milczał co najmniej do 2 doby
  // [PRÓG] po ostatnim pełnym odczycie (`ab_bl`, z dokładnością do godziny). Brak zasilania panelu to nie wyjazd.
  void wyjazdB15(const std::vector<DzienListy>& l) {
    if (!zegar()) return;
    uint32_t pelny = a.nvs.liczbaU("cpap", "ab_pelny", 0), bl = a.nvs.liczbaU("cpap", "ab_bl", 0);
    if (!pelny) pelny = abPoprzedni;  // pierwszy pełny odczyt po aktualizacji: jak dotąd
    abPoprzedni = 0;
    if (teraz() >= pelny + 3600 || teraz() < pelny) a.nvs.ustawLiczbeU("cpap", "ab_pelny", teraz());  // raz na godzinę
    ProgiWyjazdu pw;
    if (!pelny || bl + 3600 < pelny + (uint32_t)pw.przerwaDob * 86400u) return;
    std::string dO, dT;
    int mO = 0, mT = 0;
    dataIMinuta(pelny, dO, mO);
    dataIMinuta(teraz(), dT, mT);
    std::vector<std::string> noce;
    noce.reserve(l.size());
    for (auto& x : l) noce.push_back(dataPoranka(x.day));
    int n = oznaczPozaDomem(czynniki, noce, dO, mO, dT, mT);
    if (n) {
      zapiszCzynniki();
      log("czynniki: %d nocy „spanie poza domem” po przerwie AirBridge od %s (R-CZY-19)", n, dO.c_str());
    }
  }
  void linieB15(TrescEkranu& t, const DaneDnia& dd) {
    t.linia = dd.linia;
    t.liniaA = dd.liniaA;
    t.liniaOstrzezenie = dd.liniaOstrzezenie;
    if (t.stopkaStandardowa && t.stopkaLewo.empty() && t.stopkaPrawo.empty()) {
      t.stopkaLewo = dd.pogoda;
      t.stopkaPrawo = dd.przyLozku;
      dopasujStopke(t.stopkaLewo, t.stopkaPrawo);
    }
  }
  bool wzorzecLB15() {  // R-EKR-36: aktywny wzorzec L (także wyciszony) albo „tylko raport” ze spełnionym warunkiem
    for (auto& w : b5.wyniki)
      if (w.oceniona && w.spelniona && !w.poziom.empty() && w.stan != reguly::StanReguly::ZABLOKOWANA &&
          w.id.compare(0, 2, "L-") == 0)
        return true;
    return false;
  }
  podsumowanie::DaneS8 daneS8B15(const std::string& dzis) {
    podsumowanie::WejscieS8 w;
    w.norma = &stany(dzis).norma;
    w.tryb = trybB15();
    w.profilPubliczny = profilB18();
    w.wzorzecL = silnikDzis(dzis) && wzorzecLB15();
    return podsumowanie::budujS8(w);
  }
  void rysujLiczbyB15(const std::string& dzis) {  // R-KOK-22: strona kokpitu bez numeru po K2 (profil publiczny)
    podsumowanie::DaneLiczb l = podsumowanie::budujLiczby(trybB15(), daneS8B15(dzis).centralne, sprawyWCiaguB5(dzis));
    Naglowek nag{l.naglowek, numerStronyTekst(Ekran::LICZBY)};  // nazwa strony zamiast daty, bez numeru
    podsumowanie::rysujLiczby(plotno(), nag, l);
  }
  bool rysujS7B15(const std::string& dzis, const DaneDnia& dd) {
    podsumowanie::DaneS7 s = podsumowanie::budujS7(dzis, hist, trybB15(), false);  // AHI w obu profilach
    TrescEkranu t = podsumowanie::trescS7(s);
    linieB15(t, dd);
    podsumowanie::rysujS7(plotno(), Naglowek{naglowekDniaLewo(dd), naglowekDniaPrawo(dd)}, s, t);
    log("ekran dnia: S7 %s, zdanie: %s", s.etykieta.c_str(), s.zdanie.empty() ? "brak (R-EKR-20)" : s.zdanie.c_str());
    dzienOdswiezalny = true;
    r.dzienTryb = 0;
    return true;
  }
  bool rysujS8B15(const std::string& dzis, const DaneDnia& dd) {
    podsumowanie::DaneS8 s = daneS8B15(dzis);
    TrescEkranu t = podsumowanie::trescS8(s);
    linieB15(t, dd);
    podsumowanie::rysujS8(plotno(), Naglowek{naglowekDniaLewo(dd), naglowekDniaPrawo(dd)}, s, t);
    log("ekran dnia: S8 „Twoja norma” (%s)", s.krok.c_str());
    dzienOdswiezalny = true;
    r.dzienTryb = 0;
    return true;
  }
  // R6 (S7) i R7 (S8, jednorazowo, R-EKR-33) przed odkryciami; kamień milowy (KM, R7) wygrywa z S8 (S8 następnego dnia).
  bool rysujR6R7B15(const std::string& typ, const DaneDnia& dd) {
    std::string dzis = dzisiaj();
    if (dzis.empty() || (grupaRano(typ) != 9 && typ != "KM")) return false;
    if (wpisDzis("S7", dzis) || podsumowanie::kandydatS7(dzis, hist)) {
      if (!wpisDzis("S7", dzis)) dopiszDziennik({dzis, "S7", dzis.substr(0, 7), 'D', "", ""});
      return rysujS7B15(dzis, dd);
    }
    if (typ == "KM") return false;
    bool pokazane = a.nvs.flaga("cpap", "b15_s8", false);
    if (wpisDzis("S8", dzis) || podsumowanie::kandydatS8(stany(dzis).norma, pokazane)) {
      if (!wpisDzis("S8", dzis)) {
        dopiszDziennik({dzis, "S8", "", 'D', "", ""});
        a.nvs.ustawFlage("cpap", "b15_s8", true);
      }
      return rysujS8B15(dzis, dd);
    }
    return false;
  }
  odkrycia::CechyNocy cechyB15(odkrycia::TrybOceny t, const std::vector<odkrycia::Pokazanie>& pok) {
    odkrycia::CechyNocy c;
    c.tryb = t;
    c.czynniki = &czynniki;
    c.pokazane = odkrycia::paryPokazane(pok);
    std::set<std::string> poza = nocePozaDomem(czynniki);  // dane sypialni tych nocy poza regułami (R-CZY-19)
    for (auto& n : sypNoce) {
      if (poza.count(n.data)) continue;
      if (!std::isnan(n.co2)) c.wartosci["co2"][n.data] = n.co2;
      if (!std::isnan(n.w)) c.wartosci["wilgotnosc"][n.data] = n.w;
    }
    return c;
  }
  reguly::Kontekst kontekstB15(const std::string& dzis, reguly::Odpowiedzi& odp) {
    reguly::Kontekst kx;
    kx.historia = &hist;
    kx.dzis = dzis;
    kx.norma = &stany(dzis).norma;
    kx.stany = &stany(dzis);
    kx.trybBiezacy = trybB15();
    b5.wykluczone = wykluczoneB16(dzis);  // czynniki (poza domem, katar) i noce okresów infekcji razem
    kx.wykluczoneNoce = &b5.wykluczone;
    odp = sprawy::odpowiedziSilnika(odpowiedzi);
    kx.odpowiedzi = &odp;
    kx.pamiec = &b5.pamiec;
    kx.zestaw = &reguly::zestawWbudowany();
    kx.poczatekLeczenia = ustawieniaB9().poczatekLeczenia;  // R-SKI-08
    return kx;
  }
  // Przegląd tygodnia (R-KON-08, 9.3): cienki punkt wejścia; do scalenia slotu tygodniowego wołany rano.
  void przegladOdkrycB15(const std::string& dzis) {
    if (!odkrycia::przegladDzis(dzis, a.nvs.tekst("cpap", "odk_prz", ""))) return;
    std::vector<odkrycia::Pokazanie> pok = odkrycia::parsujPokazania(a.nvs.tekst("cpap", "odk_pok", ""));
    reguly::Odpowiedzi odp;
    reguly::Kontekst kx = kontekstB15(dzis, odp);
    uint32_t t0 = ms();
    std::vector<reguly::Wynik> w = odkrycia::ocenOdkrycia(reguly::zestawWbudowany(), kx,
                                                          cechyB15(odkrycia::TrybOceny::PRZEGLAD, pok));
    odkrycia::Kolejka kq;
    kq.przeglad = dzis;
    int por = 0;
    for (auto& x : w) {
      if (x.zmienne.count("holm")) por++;
      if (x.oceniona && x.spelniona && x.stan == reguly::StanReguly::AKTYWNA && odkrycia::porownanieGrup(odkrycia::klucz(x)))
        kq.klucze.push_back(odkrycia::klucz(x));
    }
    a.nvs.ustawTekst("cpap", "odk_kol", odkrycia::zapiszKolejke(kq));
    a.nvs.ustawTekst("cpap", "odk_prz", dzis);
    log("odkrycia: przegląd tygodnia %s, porównań %d, w kolejce %d, %lu ms", dzis.c_str(), por, (int)kq.klucze.size(),
        (unsigned long)(ms() - t0));
  }
  bool rysujOdkrycieB15(const std::string& typ, DaneDnia& dd) {
    std::string dzis = dzisiaj();
    if (dzis.empty() || grupaRano(typ) != 9 || !silnikDzis(dzis)) return false;
    przegladOdkrycB15(dzis);
    std::vector<odkrycia::Pokazanie> pok = odkrycia::parsujPokazania(a.nvs.tekst("cpap", "odk_pok", ""));
    odkrycia::Kolejka kol = odkrycia::parsujKolejke(a.nvs.tekst("cpap", "odk_kol", ""));
    reguly::Odpowiedzi odp;
    reguly::Kontekst kx = kontekstB15(dzis, odp);
    const reguly::Zestaw& z = reguly::zestawWbudowany();
    std::vector<reguly::Wynik> dzienne = odkrycia::ocenOdkrycia(z, kx, cechyB15(odkrycia::TrybOceny::DZIENNY, pok));
    std::vector<reguly::Wynik> zKolejki;
    if (!kol.klucze.empty() && dzis > kol.przeglad)
      zKolejki = odkrycia::ocenOdkrycia(z, kx, cechyB15(odkrycia::TrybOceny::REWALIDACJA, pok));
    odkrycia::WejscieWyboru ww;
    ww.dzis = dzis;
    ww.dzienne = &dzienne;
    ww.kolejka = &zKolejki;
    ww.kol = &kol;
    ww.pokazane = &pok;
    for (auto& x : dziennik)
      if (x.typ == "S1" || x.typ == "SK21" || x.typ == "SK32") ww.pokazaniaWspolne.push_back(x.data);
    ww.rozruch = liczbaNocyWaznych(hist, dzis) < 14;
    ww.zestaw = &z;
    std::string bez;
    const reguly::Wynik* w = odkrycia::wybierz(ww, bez);
    if (!w) {
      if (bez != "brak odkrycia") log("odkrycia: bez pokazania (%s)", bez.c_str());
      return false;
    }
    odkrycia::UstawieniaTekstow u;
    u.uspokojenie = b5.stany.uspokojenie;
    u.liniaZdrowia = dd.liniaOstrzezenie;
    u.tylkoRegulyBezOceny = b5.stany.niespokoj == Stany::Niespokoj::REGULY;
    u.tylkoOcenyWylaczone = b5.stany.niespokoj == Stany::Niespokoj::OCENY;
    u.tylkoBrakDanych = b5.stany.niespokoj == Stany::Niespokoj::DANE;  //
    u.brakDanychWyciek = u.tylkoBrakDanych && b5.stany.bezOcenZWycieku;  // 5.4.13 pkt 7: przyczyna to wyciek
    if (!wSilniku) {  // R-EKR-32: krok spraw tylko przy stronie spraw (R-KOK-19); jak wejscieB16()
      bool zSilnikiem = b5.dzis == dzis && !b5.wyniki.empty();
      u.bezSilnika = !zSilnikiem;
      u.bezKartSpraw = !zSilnikiem || kartySprawB5(dzis) == 0;
    }
    odkrycia::TrescOdkrycia o = odkrycia::trescOdkrycia(z, *w, trybB15(), u, &hist);
    TrescEkranu t = podsumowanie::trescS1(o);
    linieB15(t, dd);
    Naglowek nag{naglowekDniaLewo(dd), naglowekDniaPrawo(dd)};
    std::string kh = odkrycia::kluczHumoru(*w);
    const WpisHumoru* wp = nullptr;
    for (auto& x : dziennik)
      if (x.data == dzis && x.typ == "S1") wp = &x;
    Wybor wb;
    if (wp) {
      wb.tryb = wp->tryb;
      wb.scena = wp->scena;
      wb.zdanie = wp->zdanie;
    } else {
      EkranDnia ed = ekranKarty("S1", kh, dzis, dziennik, t.krok);  // 5.7.3, R-HUM-25
      KontekstHumoru khu;
      khu.dzis = dzis;
      khu.humor = humorB32();
      khu.liniaUwagi = !t.linia.empty();
      khu.zPropozycjami = Z_PROPOZYCJAMI;
      khu.uspokojenie = b5.stany.uspokojenie;
      khu.sceny = sceny();
      wb = wybierzTryb(ed, kartaMiesci(t), dziennik, khu);
      if (o.doLekarza || o.ukryty) wb.tryb = 'D';  // R-HUM-25
      dopiszDziennik({dzis, "S1", odkrycia::klucz(*w), wb.tryb, wb.scena, wb.zdanie});
    }
    bool juz = false;
    for (auto& p : pok)
      if (p.data == dzis && p.klucz == odkrycia::klucz(*w)) juz = true;
    if (!juz) {  // R-EKR-27, 9.3 (para raz): pokazania starsze niż 30 dni zostają tylko dla porównań grup
      std::vector<odkrycia::Pokazanie> nowe;
      for (auto& p : pok)
        if (odkrycia::porownanieGrup(p.klucz) || dniMiedzy(p.data, dzis) <= 31) nowe.push_back(p);
      nowe.push_back({odkrycia::klucz(*w), dzis, odkrycia::wartoscWyniku(*w)});
      a.nvs.ustawTekst("cpap", "odk_pok", odkrycia::zapiszPokazania(nowe));
    }
    if (o.ukryty) k1OdkrycieB15 = {dzis, o.tytulK1, o.powodK1};  // R-CZY-20: pełny tytuł i powód w K1
    if (wb.tryb == 'K' && t.linia.empty()) rysujKarte(plotno(), nag, t, wb.scena);
    else podsumowanie::rysujOdkrycie(plotno(), nag, o, t);
    log("ekran dnia: odkrycie %s (%s)", odkrycia::klucz(*w).c_str(), wb.tryb == 'K' && t.linia.empty() ? "karta" : "dane");
    dzienOdswiezalny = true;
    r.dzienTryb = 0;
    return true;
  }
  // Slot nocny (U1) i przerysowanie: ekran z wpisu dziennika.
  bool rysujWpisB15(const WpisHumoru& wp, DaneDnia& d) {
    if (wp.typ == "S7") return rysujS7B15(wp.data, d);
    if (wp.typ == "S8") return rysujS8B15(wp.data, d);
    if (wp.typ != "S1") return false;
    return rysujOdkrycieB15("S2A", d);
  }

  // pytanie tak/nie albo ekran wzorca L zamiast ekranu dnia; true = narysowane. Pytanie, które minęło bez
  // odpowiedzi (termin, 30 min senności), dostaje zapis „brak odpowiedzi” (R-SKI-17, R-PIE-13).
  bool rysujDzienB5(DaneDnia& d, const Naglowek& n, const std::string& dzis, const EkranDnia& ed) {
    std::string czeka = a.nvs.tekst("cpap", "pyt_tn", "");
    std::string teraz_ = b5.pytanie.jest ? b5.pytanie.data + ";" + sprawy::nazwaPytania(b5.pytanie.pytanie) : "";
    if (!czeka.empty() && czeka != teraz_) {
      sprawy::Odpowiedz o;
      if (sprawy::parsujOdpowiedz(czeka + ";brak", o) && !sprawy::jestOdpowiedz(odpowiedzi, o.data, o.pytanie)) {
        sprawy::zapiszOdpowiedz(odpowiedzi, o);
        zapiszOdpowiedzi();
        log("pytanie %s: brak odpowiedzi", czeka.c_str());
      }
      a.nvs.ustawTekst("cpap", "pyt_tn", "");
    }
    if (b5.pytanie.jest) {
      a.nvs.ustawTekst("cpap", "pyt_tn", teraz_);
      if (b5.pytanie.pytanie == sprawy::Pytanie::SENNOSC && a.nvs.tekst("cpap", "sen_d", "") != dzis) {
        a.nvs.ustawTekst("cpap", "sen_d", dzis);
        a.nvs.ustawLiczbeU("cpap", "sen_od", (uint32_t)(minutaDoby() + 1));
      }
      if (!rysujPytanieB16(d, dzis)) {  // S-K-30 z pytaniem, `Wymieniona poduszka?` z liczbami
        DanePytaniaTN tn = trescPytaniaTN(b5.pytanie.pytanie, d, b5.termin, b5.pytanie.przerwaNocy);
        if (b5.pytanie.pytanie == sprawy::Pytanie::WYJAZD) {  // R-CZY-22
          OkresWyjazdu ow;
          ow.od = b5.pytanie.data;
          ow.nocy = b5.pytanie.przerwaNocy;
          TekstyWyjazdu tw = tekstyWyjazdu(ow, b5.termin);
          tn.etykieta = tw.etykieta;
          tn.tytul = tw.tytul;
          tn.opis = tw.opis;
          tn.stopka = tw.stopka;
        }
        rysujPytanieTN(plotno(), tn);
      }
      dzienOdswiezalny = false;
      r.dzienTryb = 0;
      log("ekran dnia: pytanie %s (5.12.8)", sprawy::nazwaPytania(b5.pytanie.pytanie));
      return true;
    }
    // R-POR-24: Portret pod ▶ ustalony przed rysowaniem, żeby stopka ekranu problemu podpisała ▶ tam, dokąd prowadzi
    // (`▶ portret nocy`, R-PRZ-04); ten sam warunek co po rysowaniu w rysujDzienB15Wewn()
    r.portretStrona = portretRanoDostepny(dzis) && sekundaDzis(r.slotDziennyEp) < 0 ? 1 : 0;
    if (rysujB16(ed, d, n, 0)) return true;  // R4…R5b i S-K-31 przed ekranem L-09 (7.2)
    if (const sprawy::SprawaL* e = sprawaB5(b5.kluczEkranu)) {
      // problem poranny (S3 zdarzenia E, L-09) to ekran danych dnia ze strefą linii uwagi (5.4.5); linia
      // zaproszenia odpada, bo ekran z krokiem jest już prośbą tego dnia (R-PIE-10, R-PIE-11)
      DaneDnia dz = d;
      std::vector<kolejka::LiniaUwagi> lp;
      if (e->ton == sprawy::Ton::PROBLEM) {
        for (auto& l : b5.linie)
          if (l.zrodlo != "zaproszenie") lp.push_back(l);
        kolejka::ustawLinie(dz, lp);
      }
      if (e->ton != sprawy::Ton::PROBLEM) {  // R-PIE-07: na ekranie wzorca A jedyną linią jest E-05 krytyczne
        for (auto& l : b5.linie)
          if (l.zrodlo == "E-05/krytyczne") lp = {l};
        kolejka::ustawLinie(dz, lp);  // ostrzeżenie (R-EKR-52) z LiniaUwagi::ostrzezenie
      }
      // R-EKR-61, karta 2 pkt 6: ekran L z pilnością A albo B bez ikony aktualizacji i bez automatu tego dnia
      ekranDniaAB = e->jestL() && (e->pilnosc == reguly::Pilnosc::A || e->pilnosc == reguly::Pilnosc::B);
      if (ekranDniaAB) ustawIkoneAktualizacji(false);
      DaneWzorca dw = trescWzorca(*e, dz);
      dw.stopkaDalej = stopkaDalejPortretu();  // R-POR-24: `▶ portret nocy` zamiast `▶ kokpit`
      if (e->ton != sprawy::Ton::PROBLEM && !lp.empty()) {
        dw.linia = dz.linia;
        dw.liniaOstrzezenie = dz.liniaOstrzezenie;
      }
      rysujWzorzec(plotno(), dw);
      pokazanieB11(e->klucz, dzis);  // R-PIE-08: rytm „co 3 dni” / „co 7 dni” / R-KON-14 od tego pokazania
      if (!lp.empty()) pokazanieB11(kolejka::kluczPokazaniaLinii(lp.front()), dzis);
      dzienOdswiezalny = false;  // ekran L ma stopkę z akcjami, bez przerysowań R-EKR-60
      r.dzienTryb = 0;
      if (e->jestE()) log("ekran dnia: zdarzenie %s (S3)", e->klucz.c_str());
      else log("ekran dnia: wzorzec %s (%s)", e->klucz.c_str(), e->ton == sprawy::Ton::PROBLEM ? "problem" : "lekarz");
      if (!lp.empty()) log("linia uwagi: %s (%s)", dz.linia.c_str(), lp.front().zrodlo.c_str());
      buzzerB5(dzis);
      return true;
    }
    r.portretStrona = 0;  // bez ekranu problemu: Portret pod ▶ ustawia rysujPortretB40() (drugi ekran poranka)
    return false;
  }
  // R-POR-24, R-PRZ-04: podpis ▶ na ekranie dnia, gdy ▶ otwiera stronę Portretu; "" = zwykły podpis ekranu
  std::string stopkaDalejPortretu() {
    if (!r.portretStrona) return "";
    std::string s = sk::tekst("S11.stopka_dalej");
    log("stopka: %s (R-POR-24)", s.c_str());
    return s;
  }
  // R-POR-24 (decyzja właściciela projektu 11.11.2026): prawa część stopki ekranu z pogodą i odczytem czujnika, gdy ▶ otwiera
  // Portret: skrócony podpis `▶ portret` (tekst `S11.stopka_dalej_krotki`), odstęp jak między grupami stopki (5 spacji
  // P22B = 25 px), potem odczyt czujnika bez zmian; bez odczytu sam podpis
  std::string prawoZPortretem(const std::string& odczyt) {
    std::string s = sk::tekst("S11.stopka_dalej_krotki");
    if (odczyt.compare(0, s.size(), s) == 0) return odczyt;  // już z podpisem (dane dnia drugiego ekranu poranka)
    std::string w = odczyt.empty() ? s : s + "     " + odczyt;
    log("stopka z odczytem: „%s” (R-POR-24)", w.c_str());
    return w;
  }
  sprawy::WejscieSpraw wejscieSprawB5(const std::string& dzis, std::vector<sprawy::SprawaMetryki>& m,
                                      reguly::Odpowiedzi& odp) {
    sprawy::WejscieSpraw ws;
    ws.dzis = dzis;
    ws.sprawy = &b5.sprawy;
    m = sprawy::sprawyMetryk(hist, stany(dzis), dzis);
    ws.metryki = &m;
    ws.linieZdrowia = kolejka::linieZdrowiaBezSprawy(b5.linie);
    odp = sprawy::odpowiedziSilnika(odpowiedzi);
    ws.odpowiedzi = &odp;
    ws.ekranDnia = sprawaB5(b5.kluczEkranu);
    ws.ostatniaNiepewna = stany(dzis).ostatniaNiepewna;
    // ADR-0029 pkt 7, R-KOK-21: `Ocena: pełna` tylko przy tym samym stanie, przy którym nagłówek K1 nie ma dopisku
    ws.ocenaPelna = !stanyOcenyB5(dzis).ocenaNiepelna;
    return ws;
  }
  void zapiszKarteB5(int karta) {  // karta „Otwartych spraw” w NVS (przeżywa sen), zapis tylko przy zmianie
    b5.karta = karta;
    if ((int)a.nvs.liczbaU("cpap", "b5_karta", 1) != karta) a.nvs.ustawLiczbeU("cpap", "b5_karta", (uint32_t)karta);
  }
  int kartySprawB5(const std::string& dzis) {
    std::vector<sprawy::SprawaMetryki> m;
    reguly::Odpowiedzi o;
    return (int)sprawy::kartySpraw(wejscieSprawB5(dzis, m, o)).size();
  }
  // R-KOK-19, R-KOK-20: „Otwarte sprawy” w ciągu stron kokpitu tylko przy co najmniej jednej sprawie z R-EKR-48;
  // bez wyników silnika (zestaw nie działa, brak daty) strony nie ma, a K1 mówi `ocena częściowa` (R-KOK-21)
  bool sprawyWCiaguB5(const std::string& dzis) { return !dzis.empty() && silnikDzis(dzis) && kartySprawB5(dzis) > 0; }
  // Stany z oceną uspokojenia z wyników silnika (5.4.13 pkt 1, 4, 7), gdy silnik ocenił dziś; inaczej bez niej
  // (punkty 1, 4, 7 nieoceniane). Wspólne dla nagłówka K1 (R-KOK-21) i stanu pustego spraw (`ocenaPelna`).
  const Stany& stanyOcenyB5(const std::string& dzis) { return b5.wyniki.empty() || b5.dzis != dzis ? stany(dzis) : b5.stany; }
  // sprawa ostatniego „wiem” (po śnie: ta z „wiem” z dziś)
  const sprawy::SprawaL* sprawaWiemB5(const std::string& dzis) {
    if (const sprawy::SprawaL* s = sprawaB5(b5.kluczWiem)) return s;
    for (auto& x : b5.sprawy)
      if (x.wiem == dzis) return &x;
    return nullptr;
  }
  bool cofniecieB5() {  // R-PRZ-19: w nocy cofnięcie tylko przez czas ekranu (30 s „Przeniesione”, 60 s „Zapisano”)
    if (!slotNocny(minutaDoby())) return true;
    long g = ekran() == Ekran::PRZENIESIONE ? 30 : 60;
    return zegar() && (long)(teraz() - r.ekranOd) < g;
  }
  // ---------- raport dla lekarza (5.13; src/raport.h, src/serwer_raportu.h, src/raport_ekran.h) ----------
  // ▶ na ekranie L (raport tematyczny) albo w K2 (ogólny): treść i PDF składane przy otwarciu, zapis w /raporty/,
  // Wi-Fi i serwer na 15 min (wariant PDF) albo ekran-ściąga z kodem do EDF (`raport=sciaga`, R-LEK-18).
  struct StanRaportuB13 {
    bool otwarty = false, serwer = false, bezWifi = false, sciaga = false, przerysuj = false;
    std::string blad, temat, plik, stan, pobrano;
    Ekran z = Ekran::DZIEN;  // ekran, z którego otwarto (R-LEK-04)
    raport::Raport tresc;
    serwer::Sesja sesja;
    uint32_t koniecEp = 0;
  } rap;
  bool wifiRaportuB13() {
    if (a.siec.polaczona()) return true;
    if (!wifiOdMs) wifiOdMs = ms() ? ms() : 1;
    wifiProbowano = true;
    bool prz = false;
    return a.siec.polacz(LIMIT_WIFI_MS, [this]() {
      odnow();
      return false;
    }, prz);
  }
  // Paczka EDF z AirBridge do decyzji: do tego czasu raport, strona i ściąga nie obiecują paczki.
  static constexpr bool EDF_DOSTEPNA = false;
  void otworzRaportB13(const std::string& dzis, Ekran z) {
    zamknijRaportB13();
    rap = StanRaportuB13();
    rap.otwarty = true;
    rap.z = z;
    const sprawy::SprawaL* s = z == Ekran::K2 || dzis.empty() || !silnikDzis(dzis) ? nullptr : sprawaB5(b5.kluczEkranu);
    std::string konf;
    if (fsOk()) a.pliki.czytaj("/konfiguracja.txt", konf);
    raport::Wejscie we;
    we.dzis = dzis;
    we.czas = godzina(teraz());
    we.historia = &hist;
    we.wykluczone = &b5.wykluczone;
    if (!dzis.empty()) we.stany = &stany(dzis);
    we.temat = s;
    we.sprawy = &b5.sprawy;
    we.wyniki = &b5.wyniki;
    we.pamiec = &b5.pamiec;
    const reguly::Zestaw* zw = sprawy::zrodloTestowe.zestaw;
#if CPAP_SILNIK_REGUL
    if (!zw) zw = &reguly::zestawWbudowany();
#endif
    we.zestaw = zw;
    reguly::Tryb tryb = trybHist();
    if (sprawy::zrodloTestowe.zestaw) we.tekst = sprawy::zrodloTestowe.tekst;
    else if (zw) we.tekst = [zw, tryb](const std::string& id, const reguly::Wynik& w) { return reguly::tekst(*zw, id, w, tryb); };
    we.odpowiedzi = &odpowiedzi;
    we.czynniki = &czynniki;
    we.czynnikiWlaczone = raport::czynnikiZKonfiguracji(konf, we.czynnikiUstawione);
    we.podpowiedziH = ustawieniaB9().podpowiedz != 0;  // R-HIP-07, `podpowiedz=nie` = bez strefy 3b
    we.wersjaPanelu = a.uruchomienie.wersja();
    we.edf = EDF_DOSTEPNA;
    we.wersjaAirBridge = r.ab.wersja;
    rap.tresc = raport::zbuduj(we);
    rap.temat = rap.tresc.temat;
    rap.sciaga = raport::wariantZKonfiguracji(konf) == raport::Wariant::SCIAGA;
    if (hist.empty() || dzis.empty()) rap.blad = raport::KOD_BRAK_DANYCH;
    if (rap.blad.empty() && !rap.sciaga) {  // PDF w /raporty/ przed włączeniem serwera (R-LEK-05, R-LEK-23)
      std::string b;
      std::string pdf = raport::pdf(rap.tresc, b);
      std::vector<std::string> ist;
      if (b.empty() && fsOk()) a.pliki.listuj("/raporty", ist);
      if (b.empty()) rap.plik = raport::nazwaPliku(rap.tresc, dzis, ist);
      if (b.empty() && (!fsOk() || !a.pliki.zapiszAtomowo(rap.plik.c_str(), pdf))) b = raport::KOD_ZAPIS;
      rap.blad = b;
      if (b.empty()) log("raport dla lekarza: %s, %u B", rap.plik.c_str(), (unsigned)pdf.size());
    }
    if (!rap.blad.empty()) {
      log("raport dla lekarza: nie udało się (%s)", rap.blad.c_str());
      return;
    }
    if (s) {  // R-LEK-14, R-LEK-26 (także otwarcie ściągi z ekranu L)
      wzorce::raportZEkranuL(b5.pamiec, s->klucz, dzis);
      zapiszPamiecRegul();
    }
    if (rap.sciaga && !EDF_DOSTEPNA) {  // ściąga podaje tylko paczkę EDF: bez niej radio i serwer nic nie dają
      log("raport dla lekarza: ściąga bez paczki EDF, bez Wi-Fi i serwera");
      return;
    }
    serwer::GniazdoHttp* g = serwer::gniazdo();
    // R-NF-09, R-NF-27: serwer tylko pod adresem z sieci prywatnej
    if (!g || !wifiRaportuB13() || !serwer::adresPrywatny(a.siec.adresIp()) || !g->start(80)) {  // R-LEK-22 (panel bez karty: bez ścieżki)
      rap.bezWifi = true;
      log("raport dla lekarza: nie mogę włączyć Wi-Fi albo serwera");
      zamknijSiec();
      return;
    }
    uint8_t bajty[serwer::BAJTY_TOKENU];  // T-WYM-35: z generatora sprzętowego po włączeniu radia
    for (int i = 0; i < serwer::BAJTY_TOKENU; i += 4) {
      uint32_t x = a.siec.losowa();
      for (int j = 0; j < 4; j++) bajty[i + j] = (uint8_t)(x >> (8 * j));
    }
    std::vector<std::string> ist;
    if (fsOk()) a.pliki.listuj("/raporty", ist);
    rap.sesja.otworz(serwer::token(bajty), ms(), a.siec.adresIp(), rap.sciaga ? serwer::Tryb::SCIAGA : serwer::Tryb::PDF,
                     rap.plik, serwer::pozycjeZPlikow(ist), dzis, rap.temat,
                     ddmm(rap.tresc.od) + "–" + ddmm(rap.tresc.do_), EDF_DOSTEPNA);
    rap.serwer = true;
    rap.koniecEp = teraz() + serwer::CZAS_MS / 1000;
    log("raport dla lekarza: serwer do %s (%s)", godzina(rap.koniecEp).c_str(), rap.sciaga ? "ściąga" : "PDF");
  }
  void zamknijRaportB13() {
    if (!rap.serwer) return;
    if (serwer::gniazdo()) serwer::gniazdo()->stop();
    rap.serwer = false;
    rap.sesja.zamknij();
    log("raport dla lekarza: serwer wyłączony");
    if (!otaAktywne) zamknijSiec();  // R-LEK-04, R-LEK-11
  }
  // W pętli czuwania: żądania telefonu; po 15 min albo 5 złych żądaniach powrót do ekranu, z którego otwarto.
  void obsluzRaportB13() {
    if (!rap.serwer) return;
    if (!rap.sesja.aktywna(ms())) {
      log(rap.sesja.zamknietaZlymi() ? "raport dla lekarza: 5 złych żądań (R-LEK-03b)" : "raport dla lekarza: minęło 15 min");
      zamknijRaportB13();
      if (ekran() == Ekran::RAPORT) {
        ustawEkran(rap.z);
        r.ekranOd = teraz();
        pokaz();
      }
      return;
    }
    serwer::gniazdo()->obsluz([this](const std::string& sc) {
      serwer::Odpowiedz o = rap.sesja.obsluz(sc, ms());
      if (!o.plik.empty() && !(fsOk() && a.pliki.czytaj(o.plik.c_str(), o.cialo))) {
        o = serwer::Odpowiedz();
        o.cialo = "Nie ma takiej strony.";
      }
      if (o.edf && !rap.sesja.edf()) {  // paczki EDF nie ma: bez udawania awarii AirBridge
        o.kod = 501;
        o.cialo = serwer::EDF_NIEDOSTEPNA;
      }
      if (o.pdfPobrany && o.kod == 200) {
        rap.pobrano = "Pobrano o " + godzina(teraz()) + ".";  // R-LEK-21
        rap.przerysuj = true;
      }
      return o;
    });
    if (rap.przerysuj && ekran() == Ekran::RAPORT) {
      rap.przerysuj = false;
      pokaz();
    }
  }
  void rysujRaportB13(const std::string& dzis) {
    DaneDnia d = daneDnia(dzis);
    std::string dz = dataOk(dzis) ? std::string(DNI_KROTKO[dzienTygodnia(dzis)]) + " " + ddmm(dzis) : "";
    Naglowek n = {dz.empty() ? "raport gotowy" : dz + " · raport gotowy", "Wi-Fi włączone"};
    if (!rap.otwarty || !rap.blad.empty()) {  // R-LEK-05
      DaneRaportu e;
      e.naglowek = {naglowekDniaLewo(d), ""};
      e.temat = rap.otwarty ? rap.temat : "Raport dla lekarza";
      e.kod = rap.otwarty ? rap.blad : raport::KOD_BRAK_DANYCH;
      rysujRaport(plotno(), e);
      return;
    }
    if (rap.bezWifi) {
      rysujRaportBezWifi(plotno(), {naglowekDniaLewo(d), ""}, rap.temat, "");
      return;
    }
    if (rap.sciaga) {
      DaneSciagi s;
      s.naglowek = n;
      s.temat = rap.temat;
      s.pytanie = rap.tresc.sciagaPytanie;
      for (int i = 0; i < 3; i++) {
        s.liczby[i] = rap.tresc.sciagaLiczby[i];
        s.etykiety[i] = rap.tresc.s4tab.kolumny.size() > (size_t)i + 1 ? rap.tresc.s4tab.kolumny[(size_t)i + 1] : "";
      }
      s.podpis = rap.tresc.sciagaPodpis;
      s.adresQr = rap.serwer && rap.sesja.edf() ? rap.sesja.adresQr() : "";  // kod i podpis tylko z paczką EDF
      rysujSciage(plotno(), s);
      return;
    }
    DaneEkranuRaportu e;
    e.naglowek = n;
    e.temat = rap.temat;
    e.okres = "Okres: " + ddmm(rap.tresc.od) + "–" + ddmm(rap.tresc.do_) + " · 1 strona A4";
    e.adresQr = rap.sesja.adresQr();
    e.adresReczny = rap.sesja.adresReczny();
    e.kodWazny = "Kod ważny do " + godzina(rap.koniecEp) + ".";
    e.stan = rap.stan;
    e.pobrano = rap.pobrano;
    e.starsze = rap.sesja.starsze();
    rysujRaportQr(plotno(), e);
  }

  // ---------- aktualizacja z wydań GitHub (ADR-0032, ADR-0038, src/aktualizacja.h; U-07, R-NF-32…R-NF-36,
  // R-KONF-43…R-KONF-46, R-EKR-61) ----------
  // Stan trwały w NVS `aktual` (aktualizacja::Stan) i stan ekranu U-07 (`u07`, `u07_p`, `u07_x`, `u07_op`). Token
  // wydań tylko z NVS `sekrety/gh_tok`: nigdy w logu ani na ekranie (R-NF-36); bez tokenu zero zapytań (R-NF-33).
  aktualizacja::Stan akt;
  bool aktWczytany = false;
  bool ekranDniaAB = false;       // ostatni ekran dnia to L z pilnością A albo B
  bool automatCzeka = false;      // dobowe sprawdzenie znalazło nowszą wersję, a automat może jej spróbować po rysowaniu
  bool koniecPoRestarcie = false; // instalacja zakończona restartem (atrapa wraca z restart(): bez snu)
  bool u07Teraz = false;
  bool u07Oczekuje = false;       // po restarcie z ręcznej instalacji wynik dopiero po samosprawdzeniu
  bool u07Przerysuj = false;      // wynik „Zainstalowano” do narysowania po samosprawdzeniu          // ekran pośredni U-07 (sprawdzanie, pobieranie, instalacja) rysowany w tej chwili
  aktualizacja::PoStarcie aktPoStarcie = aktualizacja::PoStarcie::NIC;
  void wczytajAkt() {
    if (aktWczytany) return;
    akt = aktualizacja::wczytaj(a.nvs);
    aktWczytany = true;
  }
  void zapiszAkt() { aktualizacja::zapisz(a.nvs, akt); }
  std::string wlasnaWersja() { return aktualizacja::bezBudowy(wersjaWydania(a.uruchomienie.wersja())); }
  bool tokenWydan() { return !sekrety::kluczWydan(a.nvs).empty(); }
  bool automatWlaczony() {  // `aktualizacja_auto=tak` w /konfiguracja.txt (5.6.4, formularz U-03)
    std::string t = konfiguracjaPliku();
    size_t p = 0;
    while (p < t.size()) {
      size_t e = t.find('\n', p);
      std::string l = t.substr(p, e == std::string::npos ? std::string::npos : e - p);
      obetnijKoniec(l);
      if (l == "aktualizacja_auto=tak") return true;
      if (e == std::string::npos) break;
      p = e + 1;
    }
    return false;
  }
  bool ikonaAktualizacjiTeraz() {  // ten sam warunek przy automacie i bez niego
    wczytajAkt();
    return aktualizacja::ikonaAktualizacji(akt, wlasnaWersja(), tokenWydan());  // bez klucza bez ikony
  }
  aktualizacja::DanePanelu panelAkt() {
    aktualizacja::DanePanelu p;
    p.wersja = wlasnaWersja();
    p.uklad = ukladWkompilowany();
    p.slot = a.wydania ? a.wydania->rozmiarSlotu() : 0;
    p.procentBaterii = procentBat;
    return p;
  }
  // U-07 w NVS (przeżywa sen: wybór „jest nowsza” czeka do 5 min, R-KONF-44)
  void ustawU07(aktualizacja::StanU07 st, aktualizacja::Powod pw, const std::string& x, int op) {
    a.nvs.ustawLiczbeU(aktualizacja::PRZESTRZEN, "u07", (uint32_t)st);
    a.nvs.ustawLiczbeU(aktualizacja::PRZESTRZEN, "u07_p", (uint32_t)pw);
    a.nvs.ustawTekst(aktualizacja::PRZESTRZEN, "u07_x", x);
    a.nvs.ustawLiczbeU(aktualizacja::PRZESTRZEN, "u07_op", (uint32_t)op);
  }
  aktualizacja::StanU07 stanU07() {
    uint32_t v = a.nvs.liczbaU(aktualizacja::PRZESTRZEN, "u07", 0);
    return v <= (uint32_t)aktualizacja::STAN_U07_OSTATNI ? (aktualizacja::StanU07)v : aktualizacja::StanU07::SPRAWDZANIE;
  }
  aktualizacja::DaneU07 daneU07() {
    wczytajAkt();
    aktualizacja::DaneU07 d;
    d.stan = stanU07();
    uint32_t pw = a.nvs.liczbaU(aktualizacja::PRZESTRZEN, "u07_p", 0);
    d.powod = pw <= (uint32_t)aktualizacja::POWOD_OSTATNI ? (aktualizacja::Powod)pw : aktualizacja::Powod::BRAK;
    d.nowa = a.nvs.tekst(aktualizacja::PRZESTRZEN, "u07_x", "");
    d.wlasna = wlasnaWersja();
    d.sprawdzono = akt.sprawdzonoEp ? chwila(akt.sprawdzonoEp) : "";
    d.procentBaterii = procentBat;
    // ekran pobierania albo instalacji przerwany snem albo restartem bez wyniku: tylko ekran aktualnej wiedzy
    if (!u07Teraz && !u07Oczekuje && (d.stan == aktualizacja::StanU07::POBIERANIE || d.stan == aktualizacja::StanU07::INSTALACJA ||
        d.stan == aktualizacja::StanU07::SPRAWDZANIE || d.stan == aktualizacja::StanU07::SAMOSPRAWDZENIE))
      d.stan = aktualizacja::nowsza(akt.znana, d.wlasna) ? aktualizacja::StanU07::NOWSZA : aktualizacja::StanU07::AKTUALNA;
    if (d.stan == aktualizacja::StanU07::NOWSZA && d.nowa.empty()) d.nowa = akt.znana;
    return d;
  }
  void rysujU07Teraz() {
    aktualizacja::TresciU07 t = aktualizacja::trescU07(daneU07());
    for (auto& l : t.linie) tk.linieTestu.push_back("U-07 · " + l);
    rysujAktualizacje(plotno(), t.linie, t.stopka);
  }
  void rysujU07Od(aktualizacja::StanU07 st, const std::string& x) {  // ekran pośredni z odświeżeniem e-ink
    ustawU07(st, aktualizacja::Powod::BRAK, x, 0);
    ustawEkran(Ekran::AKTUALIZACJA);
    u07Teraz = true;
    pokaz();
    u07Teraz = false;
  }

  // Sprawdzenie najnowszego wydania (R-NF-32): `releases/latest`, potem `wydanie.json` przez API zasobu. Wymaga
  // tokenu, poprawnego czasu i Wi-Fi (wołający). Wynik BRAK z `opis` i `wyd` = jest nowsza; NIE_NOWSZA = aktualna.
  aktualizacja::Powod sprawdzWydanie(aktualizacja::OpisWydania& opis, aktualizacja::Wydanie& wyd) {
    using aktualizacja::Powod;
    std::string tok = sekrety::kluczWydan(a.nvs);
    if (tok.empty()) return Powod::BRAK_TOKENU;  // R-NF-33: bez żadnego zapytania
    if (!a.wydania) return Powod::BRAK_SIECI;
    if (!zegar()) return Powod::BRAK_SIECI;      // karta 2 pkt 7: bez poprawnego czasu brak sprawdzania
    ZadanieWydania z;
    z.url = aktualizacja::adresNajnowszego(REPO_WYDAN);
    z.token = tok;
    z.limitMs = aktualizacja::LIMIT_API_MS;
    z.maksBajtow = aktualizacja::MAKS_ODPOWIEDZI_API;
    odnow();
    OdpowiedzWydania o = a.wydania->pobierzTekst(z);
    odnow();
    if (!o.certyfikatOk) {
      log("aktualizacja: certyfikat serwera spoza zestawu, bez zapytania (R-NF-32)");
      return Powod::BRAK_SIECI;
    }
    log("aktualizacja: %s → HTTP %d", z.url.c_str(), o.kod);
    if (o.kod == 401 || o.kod == 403) return Powod::TOKEN_ODRZUCONY;
    if (o.kod == 404) {  // repo bez wydań albo token bez dostępu: nie „aktualna”
      log("aktualizacja: brak wydania albo dostępu do %s (404)", REPO_WYDAN);
      return Powod::BRAK_WYDAN;
    }
    if (o.kod != 200) return Powod::BRAK_SIECI;
    if (!aktualizacja::parsujWydanie(o.cialo, wyd)) {
      log("aktualizacja: odpowiedź GitHub nieczytelna%s", o.zaDuzo ? " (za duża)" : "");
      return Powod::BRAK_SIECI;  // nic nie pobrano, więc nie „plik uszkodzony”
    }
    if (!aktualizacja::nowsza(wyd.tag, wlasnaWersja())) {
      log("aktualizacja: najnowsze wydanie %s, panel %s: bez zmian", wyd.tag.c_str(), wlasnaWersja().c_str());
      opis.wersja = aktualizacja::bezBudowy(wyd.tag);
      return Powod::NIE_NOWSZA;
    }
    ZadanieWydania zo = z;
    zo.url = wyd.opis.adresApi;
    zo.binarne = true;
    zo.maksBajtow = aktualizacja::MAKS_OPISU;
    OdpowiedzWydania oo = a.wydania->pobierzTekst(zo);
    odnow();
    if (!oo.certyfikatOk) {
      log("aktualizacja: certyfikat serwera plików wydania spoza zestawu (%s), bez pobrania", wyd.opis.adresApi.c_str());
      return Powod::BRAK_SIECI;
    }
    log("aktualizacja: opis wydania → HTTP %d", oo.kod);
    if (oo.kod == 401 || oo.kod == 403) return Powod::TOKEN_ODRZUCONY;
    if (oo.kod != 200) return Powod::BRAK_SIECI;
    if (!aktualizacja::parsujOpis(oo.cialo, opis) || opis.wersja != aktualizacja::bezBudowy(wyd.tag)) {
      log("aktualizacja: opis wydania %s nieczytelny albo z inną wersją", wyd.tag.c_str());
      return Powod::USZKODZONY;
    }
    log("aktualizacja: jest wersja %s (%llu B), panel %s", opis.wersja.c_str(), (unsigned long long)opis.rozmiar,
        wlasnaWersja().c_str());
    return Powod::BRAK;
  }
  // Po sprawdzeniu: stan znanej wersji (U-01, U-05, ikona). `dobowe`: sprawdzenie bez udziału użytkownika (doba).
  void zapiszWynikSprawdzenia(aktualizacja::Powod pw, const aktualizacja::OpisWydania& opis, bool dobowe) {
    wczytajAkt();
    std::string dzien = dobowe ? dzisiaj() : "";
    if (pw == aktualizacja::Powod::BRAK || pw == aktualizacja::Powod::NIE_NOWSZA)
      aktualizacja::poSprawdzeniu(akt, pw == aktualizacja::Powod::BRAK ? opis.wersja : "", wlasnaWersja(), teraz(), dzien);
    else if (dobowe)
      akt.sprawdzonoDnia = dzien;  // najwyżej raz na dobę także po błędzie (R-KONF-46)
    zapiszAkt();
  }

  // R-KONF-46: raz na dobę w oknie Wi-Fi otwartym z innego powodu (rano po pobraniu nocy), bez dodatkowego wybudzenia.
  aktualizacja::OpisWydania opisAuto;
  aktualizacja::Wydanie wydAuto;
  void sprawdzenieDobowe() {
    wczytajAkt();
    bool tok = tokenWydan();
    // nie w trakcie terapii (`rop` = 1 z odczytu w tej sesji): doba nie jest zużyta, sprawdzenie w kolejnym
    // oknie Wi-Fi po pobraniu nocy
    if (!aktualizacja::sprawdzicDzis(akt, dzisiaj(), a.siec.polaczona(), tok, zegar(), minutaDoby(), r.ab.rop)) return;
    aktualizacja::OpisWydania opis;
    aktualizacja::Wydanie wyd;
    aktualizacja::Powod pw = sprawdzWydanie(opis, wyd);
    log("aktualizacja: sprawdzenie dobowe: %s", pw == aktualizacja::Powod::BRAK ? "jest nowsza" : aktualizacja::nazwaPowodu(pw));
    zapiszWynikSprawdzenia(pw, opis, true);
    aktualizacja::poSprawdzeniuDobowym(akt, dzisiaj(), pw);  // błąd trwały, automat tej doby już nie pyta
    zapiszAkt();
    if (pw != aktualizacja::Powod::BRAK) return;
    opisAuto = opis;  // świeży opis: automat w tym wybudzeniu bez drugiego zapytania
    wydAuto = wyd;
    opisAutoJest = true;
  }
  bool opisAutoJest = false;
  // Sprawa A albo B na ekranie dnia stojącym na panelu: zapisana przy każdym rysowaniu ekranu dnia, więc
  // znana także w wybudzeniu bez rysowania.
  bool sprawaABNaEkranie() { return a.nvs.flaga(aktualizacja::PRZESTRZEN, "ekr_ab", false); }
  void zapiszSpraweAB(bool ab) {
    if (a.nvs.flaga(aktualizacja::PRZESTRZEN, "ekr_ab", false) != ab) a.nvs.ustawFlage(aktualizacja::PRZESTRZEN, "ekr_ab", ab);
  }
  // Warunki automatu znane bez sieci (R-KONF-45): ustawienie, token, czas, znana nowsza wersja, slot dzienny,
  // terapia (ostatni `rop`), sprawa A/B (zapisana), jedna rozpoczęta próba na dobę, pominięcie po 3 nieudanych,
  // bateria. `powod` dostaje opis odmowy do logu ("" = bez logu: automat wyłączony albo nic do zainstalowania).
  bool automatMozeTeraz(std::string& odmowa) {
    wczytajAkt();
    odmowa.clear();
    if (!automatWlaczony() || !tokenWydan() || !zegar() || !aktualizacja::nowsza(akt.znana, wlasnaWersja())) return false;
    aktualizacja::WejscieAutomatu wa;
    wa.wlaczony = true;
    wa.dzis = dzisiaj();
    wa.minutaDoby = minutaDoby();
    wa.rop = r.ab.rop;
    wa.sprawaAB = sprawaABNaEkranie();
    aktualizacja::PowodAutomatu pa = aktualizacja::decyzjaAutomatu(akt, akt.znana, wa);
    if (pa != aktualizacja::PowodAutomatu::TAK) {
      odmowa = aktualizacja::nazwaPowoduAutomatu(pa);
      return false;
    }
    if (procentBat >= 0 && procentBat < aktualizacja::PROG_BATERII) {  // E1001 bez sygnału kabla
      odmowa = aktualizacja::nazwaPowodu(aktualizacja::Powod::BATERIA);
      return false;
    }
    return true;
  }
  // Po rysowaniu ekranu dnia w wybudzeniu zegarem albo przy starcie: gdy znana jest nowsza wersja i warunki są
  // spełnione, automat sam włącza Wi-Fi (tylko wtedy, bez osobnego wybudzenia), sprawdza opis i instaluje. Odmowa
  // z warunku przejściowego (sprawdzona przed Wi-Fi) nie zużywa doby. Najwyżej
  // aktualizacja::AUTOMAT_ZAPYTAN_NA_DOBE zapytań na dobę (ponowienie tylko po braku sieci, nie wcześniej niż po
  // AUTOMAT_PRZERWA_S), po błędzie trwałym koniec prób do następnej doby. Ikona stoi niezależnie od tego.
  static constexpr uint32_t AUTOMAT_PRZERWA_S = 3600;  // [PRÓG] odstęp ponowienia po braku sieci
  void automatPoRysowaniu(PowodWybudzenia w) {
    automatCzeka = false;
    if ((w != PowodWybudzenia::ZEGAR && w != PowodWybudzenia::START) || ekran() != Ekran::DZIEN) return;
    std::string dlaczego;
    if (!automatMozeTeraz(dlaczego)) {
      if (!dlaczego.empty()) log("aktualizacja: automat bez instalacji: %s", dlaczego.c_str());
      return;
    }
    if (!opisAutoJest) {
      if (!aktualizacja::automatMozeSiec(akt, dzisiaj())) return;  // limit doby albo błąd trwały
      uint32_t ost = a.nvs.liczbaU(aktualizacja::PRZESTRZEN, "aut_ep", 0);
      if (ost && teraz() >= ost && teraz() - ost < AUTOMAT_PRZERWA_S) return;  // ponowienie po braku sieci: po przerwie
      a.nvs.ustawLiczbeU(aktualizacja::PRZESTRZEN, "aut_ep", teraz());
      aktualizacja::Powod pw = aktualizacja::Powod::BRAK_SIECI;
      if (a.siec.polaczona() || wifiRaportuB13()) pw = sprawdzWydanie(opisAuto, wydAuto);
      zapiszWynikSprawdzenia(pw, opisAuto, false);
      aktualizacja::poZapytaniuAutomatu(akt, dzisiaj(), pw);
      zapiszAkt();
      if (pw != aktualizacja::Powod::BRAK) {
        log("aktualizacja: automat bez instalacji: %s", aktualizacja::nazwaPowodu(pw));
        zamknijSiec();
        return;
      }
      opisAutoJest = true;
    }
    aktualizacja::Powod pi = aktualizacja::decyzjaInstalacji(opisAuto, panelAkt());
    if (pi == aktualizacja::Powod::INNY_PANEL) {  // stały powód: wersja pominięta do następnego wydania
      akt.probaWersja = opisAuto.wersja;
      akt.probNieudanych = aktualizacja::PROBY_MAKS;
      akt.ostatni = pi;
      zapiszAkt();
    }
    if (pi != aktualizacja::Powod::BRAK) {
      log("aktualizacja: automat bez instalacji: %s", aktualizacja::nazwaPowodu(pi));
      aktualizacja::poZapytaniuAutomatu(akt, dzisiaj(), pi, false);  // inna płytka albo układ: koniec prób doby
      zapiszAkt();
      zamknijSiec();
      return;
    }
    aktualizacja::Powod pz = zainstaluj(opisAuto, wydAuto, true);
    if (!koniecPoRestarcie && aktualizacja::bladTrwaly(pz)) {  // np. wydanie bez obrazu, zły skrót
      wczytajAkt();
      aktualizacja::poZapytaniuAutomatu(akt, dzisiaj(), pz, false);
      zapiszAkt();
    }
  }

  // Pobranie, sprawdzenie skrótu i ustawienie drugiego slotu, restart (R-NF-34, R-KONF-44, R-KONF-45). Ręcznie: ekrany
  // U-07 pobierania i instalacji, wynik na U-07; automat: bez zmiany ekranu, wynik w logu i na U-05.
  aktualizacja::Powod zainstaluj(const aktualizacja::OpisWydania& opis, const aktualizacja::Wydanie& wyd, bool automat) {
    using aktualizacja::Powod;
    wczytajAkt();
    Powod pw = Powod::BRAK;
    if (aktualizacja::nocAlboTerapia(zegar() ? minutaDoby() : -1, r.ab.rop)) pw = Powod::NOC;
    if (pw == Powod::BRAK) pw = aktualizacja::decyzjaInstalacji(opis, panelAkt());
    if (pw == Powod::BRAK && (wyd.obraz.adresApi.empty() || wyd.obraz.rozmiar != opis.rozmiar ||
                              opis.plik != aktualizacja::nazwaObrazu(opis.wersja)))
      pw = Powod::USZKODZONY;  // brak pliku obrazu w wydaniu albo inny niż w opisie
    if (pw == Powod::BRAK && !a.siec.polaczona() && !wifiRaportuB13()) pw = Powod::BRAK_SIECI;
    if (pw != Powod::BRAK) {
      log("aktualizacja: %s %s bez instalacji: %s", automat ? "automat" : "▶", opis.wersja.c_str(),
          aktualizacja::nazwaPowodu(pw));
      if (pw != Powod::NIE_NOWSZA) {
        akt.ostatni = pw;
        zapiszAkt();
      }
      if (!automat) ustawU07(pw == Powod::NIE_NOWSZA ? aktualizacja::StanU07::AKTUALNA : aktualizacja::StanU07::PO_NIEUDANEJ,
                             pw, opis.wersja, 1);
      zamknijSiec();
      return pw;
    }
    if (!automat) rysujU07Od(aktualizacja::StanU07::POBIERANIE, opis.wersja);
    aktualizacja::zacznijProbe(akt, opis.wersja, wlasnaWersja(), dzisiaj(), automat);
    zapiszAkt();
    log("aktualizacja: pobieram %s (%s, próba %d)", opis.plik.c_str(), automat ? "automat" : "▶", akt.probNieudanych);
    ZadanieWydania z;
    z.url = wyd.obraz.adresApi;
    z.token = sekrety::kluczWydan(a.nvs);
    z.binarne = true;
    z.limitMs = aktualizacja::LIMIT_OBRAZU_MS;
    WynikObrazu wo = a.wydania->pobierzObraz(z, opis.rozmiar, [this]() { odnow(); });
    odnow();
    Powod blad = Powod::BRAK;
    if (!wo.http.certyfikatOk) log("aktualizacja: certyfikat serwera plików wydania spoza zestawu, obraz niepobrany");
    if (!wo.http.certyfikatOk || wo.http.kod != 200) blad = wo.http.kod == 401 || wo.http.kod == 403 ? Powod::TOKEN_ODRZUCONY
                                                                                                     : Powod::BRAK_SIECI;
    else if (!wo.zapisOk || wo.bajtow != opis.rozmiar || wo.sha256 != opis.sha256) blad = Powod::USZKODZONY;
    log("aktualizacja: pobrano %llu z %llu B, HTTP %d, skrót %s", (unsigned long long)wo.bajtow,
        (unsigned long long)opis.rozmiar, wo.http.kod, wo.sha256 == opis.sha256 ? "zgodny" : "niezgodny");
    if (blad == Powod::BRAK) {
      if (!automat) rysujU07Od(aktualizacja::StanU07::INSTALACJA, opis.wersja);
      if (!a.wydania->ustawStartowy()) blad = Powod::USZKODZONY;
    } else {
      a.wydania->porzuc();
    }
    if (blad != Powod::BRAK) {
      aktualizacja::nieudanaProba(akt, blad);
      zapiszAkt();
      log("aktualizacja: nieudana: %s; zostaje %s", aktualizacja::nazwaPowodu(blad), wlasnaWersja().c_str());
      if (!automat) ustawU07(aktualizacja::StanU07::PO_NIEUDANEJ, blad, opis.wersja, 1);
      zamknijSiec();
      return blad;
    }
    // slot ustawiony: instalacja w toku w NVS (in_z, in_na) przed restartem, żeby nowa wersja albo powrót znały wynik
    ustawU07(aktualizacja::StanU07::INSTALACJA, Powod::BRAK, opis.wersja, 1);
    zapiszAkt();
    zapiszWszystko();
    zamknijSiec();
    log("aktualizacja: wersja %s w drugim slocie, restart", opis.wersja.c_str());
    koniecPoRestarcie = true;
    a.wydania->restart();
    return Powod::BRAK;
  }

  // ↻ 2 s na U-01 i ▶ „spróbuj ponownie” po nieudanym sprawdzeniu (R-KONF-43): U-07 ze sprawdzeniem.
  void akcjaSprawdz() {
    wczytajAkt();
    if (!tokenWydan()) {  // R-NF-33: bez Wi-Fi i bez zapytania
      log("aktualizacja: brak klucza wydań, bez zapytania (R-NF-33)");
      ustawU07(aktualizacja::StanU07::WYLACZONE, aktualizacja::Powod::BRAK_TOKENU, "", 0);
      return;
    }
    rysujU07Od(aktualizacja::StanU07::SPRAWDZANIE, "");
    aktualizacja::OpisWydania opis;
    aktualizacja::Wydanie wyd;
    aktualizacja::Powod pw = aktualizacja::Powod::BRAK_SIECI;
    if (wifiRaportuB13()) {
      if (!zegar()) a.siec.czasZSieci(LIMIT_NTP_MS);  // HTTPS wymaga czasu (karta 2 pkt 7)
      pw = sprawdzWydanie(opis, wyd);
    }
    zapiszWynikSprawdzenia(pw, opis, false);
    zamknijSiec();
    if (pw == aktualizacja::Powod::BRAK) ustawU07(aktualizacja::StanU07::NOWSZA, pw, opis.wersja, 0);
    else if (pw == aktualizacja::Powod::NIE_NOWSZA) ustawU07(aktualizacja::StanU07::AKTUALNA, pw, "", 0);
    else ustawU07(aktualizacja::StanU07::SPRAWDZENIE_NIEUDANE, pw, "", 0);  // tylko powód sprawdzenia
  }
  // ▶ „tak, instaluj” (R-KONF-44): świeże sprawdzenie (opis mógł się zmienić w czasie snu), potem instalacja.
  void akcjaInstaluj() {
    wczytajAkt();
    if (!tokenWydan()) {
      ustawU07(aktualizacja::StanU07::WYLACZONE, aktualizacja::Powod::BRAK_TOKENU, "", 1);
      return;
    }
    aktualizacja::OpisWydania opis;
    aktualizacja::Wydanie wyd;
    aktualizacja::Powod pw = aktualizacja::Powod::BRAK_SIECI;
    if (wifiRaportuB13()) {
      if (!zegar()) a.siec.czasZSieci(LIMIT_NTP_MS);
      pw = sprawdzWydanie(opis, wyd);
    }
    zapiszWynikSprawdzenia(pw, opis, false);
    std::string zPytania = a.nvs.tekst(aktualizacja::PRZESTRZEN, "u07_x", "");
    if (pw == aktualizacja::Powod::BRAK && !zPytania.empty() && opis.wersja != zPytania) {
      // w czasie pytania wyszła inna wersja: pytanie od nowa, bez instalacji
      log("aktualizacja: w pytaniu %s, teraz najnowsza %s: pytam ponownie", zPytania.c_str(), opis.wersja.c_str());
      zamknijSiec();
      ustawU07(aktualizacja::StanU07::NOWSZA, aktualizacja::Powod::BRAK, opis.wersja, 0);
      return;
    }
    if (pw == aktualizacja::Powod::BRAK) {
      zainstaluj(opis, wyd, false);
      return;
    }
    zamknijSiec();
    if (pw == aktualizacja::Powod::NIE_NOWSZA) ustawU07(aktualizacja::StanU07::AKTUALNA, pw, "", 1);
    else ustawU07(aktualizacja::StanU07::SPRAWDZENIE_NIEUDANE, pw, "", 1);  // tylko powód sprawdzenia
  }
  // Po starcie: wynik instalacji z poprzedniego uruchomienia (R-KONF-44 `Zainstalowano…`, R-NF-35 powrót).
  // ZAINSTALOWANO zapisuje się dopiero przy wersji dobrej (po samosprawdzeniu), żeby powrót do poprzedniej rozpoznał
  // nieudaną instalację; NIE_WSTALA od razu.
  void aktualizacjaPoStarcie(PowodWybudzenia w) {
    if (w != PowodWybudzenia::START) return;
    wczytajAkt();
    if (akt.instalujeNa.empty()) return;
    aktualizacja::Stan kopia = akt;
    bool reczna = !akt.automatyczna;
    aktPoStarcie = aktualizacja::poStarcie(kopia, wlasnaWersja());
    if (aktPoStarcie == aktualizacja::PoStarcie::ZAINSTALOWANO) {
      log("aktualizacja: działa nowa wersja %s", wlasnaWersja().c_str());
      if (!samosprawdzenieWymagane(a.uruchomienie.stanWersji())) {
        potwierdzInstalacje();
        if (reczna) ustawU07(aktualizacja::StanU07::PO_UDANEJ, aktualizacja::Powod::BRAK, wlasnaWersja(), 1);
      } else if (reczna) {
        // do samosprawdzenia U-07 `Sprawdzam nową wersję…` (restart już był); `Zainstalowano` dopiero po
        // oznaczeniu wersji jako dobrej
        ustawU07(aktualizacja::StanU07::SAMOSPRAWDZENIE, aktualizacja::Powod::BRAK, wlasnaWersja(), 1);
        u07Oczekuje = true;
      }
    } else if (aktPoStarcie == aktualizacja::PoStarcie::NIE_WSTALA) {
      akt = kopia;
      zapiszAkt();
      log("aktualizacja: wersja %s nie potwierdziła startu, panel wrócił do %s (R-NF-35)", akt.odrzucona.c_str(),
          wlasnaWersja().c_str());
      if (reczna) ustawU07(aktualizacja::StanU07::PO_NIEUDANEJ, aktualizacja::Powod::NIE_WSTALA, akt.odrzucona, 1);
    } else {
      akt = kopia;  // inna wersja (np. wgranie kablem): instalacja zapomniana bez wyniku
      zapiszAkt();
    }
  }
  bool ekranPoInstalacji() {  // po restarcie z ręcznej instalacji: U-07 z wynikiem zamiast ekranu dnia
    return aktPoStarcie != aktualizacja::PoStarcie::NIC && !akt.automatyczna;
  }
  void potwierdzInstalacje() {  // wersja dobra: wynik instalacji zapisany (próby i znana wersja wyczyszczone)
    if (aktPoStarcie != aktualizacja::PoStarcie::ZAINSTALOWANO || akt.instalujeNa.empty()) return;
    bool reczna = !akt.automatyczna;
    aktualizacja::poStarcie(akt, wlasnaWersja());
    zapiszAkt();
    if (u07Oczekuje && reczna) {  // teraz wolno pokazać `Zainstalowano wersję {X}.`
      ustawU07(aktualizacja::StanU07::PO_UDANEJ, aktualizacja::Powod::BRAK, wlasnaWersja(), 1);
      u07Oczekuje = false;
      u07Przerysuj = true;
    }
  }

  // ---------- U-01 „Ustawienia”, U-03 formularz z kodem QR (5.6.3, 5.6.5, R-KONF-10…R-KONF-18, R-KONF-37…39) ----------
  // Serwer formularza w domowym Wi-Fi tym samym gniazdem co raport (GniazdoHttp), tylko pod adresem prywatnym (R-NF-09),
  // token 128 bitów, 15 min, 5 złych żądań (R-KONF-13…R-KONF-15). Zapis bezpieczny: pola pliku od razu; nowe dane
  // AirBridge sprawdzone `GET /api/status` przed zapisem; nowa sieć Wi-Fi sprawdzona po odpowiedzi telefonowi (60 s
  // na połączenie, potem `GET /api/status`); nieudane sprawdzenie zostawia poprzednie dane i otwiera formularz od nowa.
  struct StanFormularzaP07 {
    bool serwer = false, bezWifi = false, przerysuj = false, sprawdzSiec = false;
    formularz::Sesja sesja;
    uint32_t koniecEp = 0;
    std::string zapisano, stan;
    std::map<std::string, std::string> bledy;
    sekrety::Dane nowe;  // sekrety do sprawdzenia po odpowiedzi (nowa sieć)
  } frm;
  sekrety::Dane sekretyTeraz() {
    sekrety::Dane d;
    d.ssid = k.ssid;
    d.hasloWifi = k.hasloWifi;
    d.abAdres = k.abAdres;
    d.abKonto = k.abKonto;
    d.abHaslo = k.abHaslo;
    return d;
  }
  void ustawSekrety(const sekrety::Dane& d) {
    k.ssid = d.ssid;
    k.hasloWifi = d.hasloWifi;
    k.abAdres = d.abAdres;
    k.abKonto = d.abKonto;
    k.abHaslo = d.abHaslo;
    abWyzwanie = WyzwanieDigest();  // inne konto albo adres: nowe wyzwanie Digest
    abNc = 0;
  }
  // U-01: bieżące ustawienia panelu do odczytu, bez haseł, konta AirBridge i tokenu (R-KONF-38). Te same
  // źródła co formularz U-03 (formularz::zPliku) i co buzzer (cisza i dźwięk z NVS etapu 1, gdy w pliku brak wiersza).
  DaneUstawien daneU01() {
    formularz::Pola f = formularz::zPliku(konfiguracjaPliku(), sekretyTeraz(), otoUzywalne(), k.pogodaLat, k.pogodaLon);
    const UstawieniaDnia& ust = ustawieniaB9();
    DaneUstawien d;
    d.ssid = f.ssid;
    d.abAdres = f.abAdres;
    d.miasto = f.miasto;
    // bez miasta z listy, ale ze współrzędnymi wgranymi z panelem (secrets.h) pogoda działa: nie `nie wybrano`
    if (d.miasto.empty() && !std::isnan(k.pogodaLat) && !std::isnan(k.pogodaLon)) d.miasto = MIASTO_ZE_WSPOLRZEDNYCH;
    d.profilPubliczny = profilB18();
    d.sprawy = sprawyWCiaguB5(dzisiaj());  // ◀ podpisany stroną przed U-01 (R-KOK-20)
    d.humor = ust.humor == 0 ? 0 : ust.humor == 1 ? 1 : 2;
    d.dzwiek = ust.buzzer >= 0 ? ust.buzzer != 0 : a.nvs.liczbaU("cpap", "dzwiek", 1) != 0;
    int od = ust.ciszaOd, doo = ust.ciszaDo;
    if (od < 0 || doo < 0) {
      od = (int)a.nvs.liczbaU("cpap", "cisza_od", 0) - 1;
      doo = (int)a.nvs.liczbaU("cpap", "cisza_do", 0) - 1;
    }
    d.ciszaOd = od < 0 || od >= 24 * 60 ? kolejka::CISZA_OD : od;
    d.ciszaDo = doo < 0 || doo >= 24 * 60 ? kolejka::CISZA_DO : doo;
    d.podpowiedz = ust.podpowiedz != 0;
    d.co2 = otoUzywalne() ? (co2B17() ? 1 : 0) : -1;
    d.wersja = wersjaWydania(a.uruchomienie.wersja());
    wczytajAkt();  // linia aktualizacji pod listą: stan wersji i droga do U-07 (↻ 2 s)
    d.aktualizacja = aktualizacja::wierszU01(tokenWydan(), akt.znana, wlasnaWersja());  // R-KONF-43, R-NF-33
    return d;
  }
  std::string konfiguracjaPliku() {
    std::string t;
    if (fsOk()) a.pliki.czytaj("/konfiguracja.txt", t);
    return t;
  }
  void otworzFormularzP07() {
    std::string stan = frm.stan;
    zamknijFormularzP07();
    frm = StanFormularzaP07();
    frm.stan = stan;
    serwer::GniazdoHttp* g = serwer::gniazdo();
    if (!g || !wifiRaportuB13() || !serwer::adresPrywatny(a.siec.adresIp()) || !g->start(80)) {  // R-NF-09
      frm.bezWifi = true;
      log("formularz: nie mogę włączyć Wi-Fi albo serwera");
      zamknijSiec();
      return;
    }
    uint8_t bajty[serwer::BAJTY_TOKENU];  // T-WYM-35, T-NF-27: generator sprzętowy przy włączonym radiu
    for (int i = 0; i < serwer::BAJTY_TOKENU; i += 4) {
      uint32_t x = a.siec.losowa();
      for (int j = 0; j < 4; j++) bajty[i + j] = (uint8_t)(x >> (8 * j));
    }
    frm.sesja.otworz(serwer::token(bajty), ms(), a.siec.adresIp());
    frm.serwer = true;
    frm.koniecEp = teraz() + serwer::CZAS_MS / 1000;
    log("formularz: serwer do %s (R-KONF-12)", godzina(frm.koniecEp).c_str());
  }
  void zamknijFormularzP07() {
    if (!frm.serwer) return;
    if (serwer::gniazdo()) serwer::gniazdo()->stop();
    frm.serwer = false;
    frm.sesja.zamknij();
    log("formularz: serwer wyłączony");
    if (!otaAktywne) zamknijSiec();  // R-KONF-15, R-KONF-16
  }
  // R-MAS-17: termin następnej wymiany przy polu daty, z datami po odpowiedzi „tak” (jak S-K-31, S-K-35)
  void terminyFormularzaB16(formularz::Pola& pf) {
    sprzet::Elementy el = sprzetB16();
    std::string dzis = dzisiaj();
    for (int e = 0; e < formularz::ELEMENTY && e < EL_N; e++) pf.termin[e] = sprzet::terminFormularza(el.el[e], dzis);
    pf.nowyModelDzis = dataOk(dzis) && a.nvs.tekst("cpap", "sp_model", "") == dzis;  // R-MAS-19: pole cofnięcia
  }
  std::string stronaP07(const std::string& komunikat) {
    formularz::Pola pf = formularz::zPliku(konfiguracjaPliku(), sekretyTeraz(), otoUzywalne(), k.pogodaLat, k.pogodaLon);
    terminyFormularzaB16(pf);
    pf.kluczWydan = tokenWydan();  // tylko „ustawiony”, nigdy wartość (R-NF-36)
    pf.hasloOkna = sekrety::hasloOknaUstawione(a.nvs);
    return formularz::strona(pf, frm.sesja.sciezka(), komunikat, frm.bledy);
  }
  // Nowe dane AirBridge w bieżącej sieci: `GET /api/status` z logowaniem przed zapisem (5.6.5 krok 3, R-KONF-37).
  std::string sprawdzAirBridgeP07(const sekrety::Dane& nowe, bool& ok) {
    sekrety::Dane stare = sekretyTeraz();
    ustawSekrety(nowe);
    std::string t;
    int kod = abGetRaz("/api/status", t);
    ok = kod == 200;
    if (ok) {
      bool zap = sekrety::zapiszZFormularza(a.nvs, nowe);  // z odczytem kontrolnym
      log("formularz: AirBridge %s odpowiada, dane %s", nowe.abAdres.c_str(), zap ? "zapisane" : "niezapisane");
      if (zap) return "AirBridge odpowiada pod " + nowe.abAdres + ". Zapisano.";
      ok = false;
      ustawSekrety(stare);
      return "AirBridge odpowiada, ale nie udało się zapisać w pamięci panelu. Poprzednie ustawienia zostają.";
    }
    ustawSekrety(stare);
    log("formularz: AirBridge %s: kod %d, zostają poprzednie dane", nowe.abAdres.c_str(), kod);
    return (kod == 401 ? "AirBridge pod " + nowe.abAdres + " odrzucił logowanie. Sprawdź konto i hasło."
                       : "AirBridge nie odpowiada pod " + nowe.abAdres + ".") +
           " Poprzednie ustawienia zostają.";
  }
  // POST z ważnym tokenem: walidacja, zapis pliku, sprawdzenie AirBridge; komunikat dla telefonu.
  std::string zapiszFormularzP07(const std::map<std::string, std::string>& pola) {
    formularz::Wynik w = formularz::waliduj(pola, sekretyTeraz(), dzisiaj());
    frm.bledy = w.bledy;
    std::string stara = konfiguracjaPliku(), nowa = formularz::scal(stara, w.plik), dopisek;
    std::string staraMaska = ustawieniaB9().sprzet[EL_MASKA].wymieniono;  // R-MAS-19: stan sprzed nowego modelu
    bool plikOk = true;
    if (nowa != stara) {
      bool ok = fsOk() && a.pliki.zapiszAtomowo("/konfiguracja.txt", nowa);
      plikOk = ok;
      ustawieniaB9_ = parsujUstawieniaDnia(nowa);  // R-KONF-23: nowa wartość do restartu także przy błędzie zapisu
      ustawieniaWczytane_ = true;
      b5.dzis.clear();
      log(ok ? "formularz: /konfiguracja.txt zapisany (R-KONF-17)" : "formularz: zapis /konfiguracja.txt nieudany");
      if (!ok) dopisek = " Nie udało się zapisać w pamięci panelu.";
    }
    if (w.nowyModelMaski) nowyModelZFormularzaB16(plikOk, staraMaska);  // R-MAS-16
    if (w.cofnijNowyModel) cofnijNowyModelZFormularzaB16();         // R-MAS-19
    // sekcja „Aktualizacje”: token wydań i hasło okna serwisowego do NVS `sekrety` (ADR-0038 pkt 4)
    if (!w.kluczWydan.empty() || w.usunKlucz) {
      bool ok = sekrety::zapiszKluczWydan(a.nvs, w.usunKlucz && w.kluczWydan.empty() ? "" : w.kluczWydan);
      log("formularz: klucz wydań %s", !ok ? "niezapisany" : w.kluczWydan.empty() ? "usunięty" : "zapisany");
      if (!ok) dopisek += " Nie udało się zapisać klucza wydań.";
    }
    if (!w.hasloOkna.empty()) {
      bool ok = sekrety::zapiszHasloOkna(a.nvs, w.hasloOkna);
      log("formularz: hasło okna serwisowego %s (działa od następnego wybudzenia)", ok ? "zapisane" : "niezapisane");
      if (!ok) dopisek += " Nie udało się zapisać hasła okna.";
    }
    std::string kom;
    if (w.zmianaSieci) {
      frm.sprawdzSiec = true;
      frm.nowe = w.sekrety;
      frm.stan = "Łączę z siecią " + w.sekrety.ssid + "…";
      kom = "Panel sprawdza sieć " + w.sekrety.ssid +
            ". Wynik zobaczysz na ekranie panelu; do tego czasu zostaje poprzednia sieć.";
    } else if (w.zmianaAirBridge) {
      bool ok = false;
      kom = frm.stan = sprawdzAirBridgeP07(w.sekrety, ok);
    }
    frm.zapisano = "Zapisano o " + godzina(teraz()) + ".";
    if (!w.ok()) kom = "Popraw pola oznaczone niżej; pozostałe zapisane o " + godzina(teraz()) + ". " + kom;
    else if (kom.empty()) kom = frm.zapisano;
    frm.przerysuj = true;
    return kom + dopisek;
  }
  // Nowa sieć Wi-Fi (R-KONF-10, R-KONF-11, R-KONF-37): poza serwerem, po odpowiedzi telefonowi. Udane połączenie
  // i `GET /api/status` → zapis w NVS; inaczej powrót do poprzedniej sieci i formularz od nowa (nowy kod).
  void sprawdzSiecP07() {
    frm.sprawdzSiec = false;
    sekrety::Dane nowe = frm.nowe, stare = sekretyTeraz();
    pauzaPrzedWylaczeniem();  // komunikat `Panel sprawdza sieć…` dochodzi do telefonu
    zamknijFormularzP07();
    a.siec.wylacz();
    pokaz();  // `Łączę z siecią {nazwa}…`
    a.siec.ustawSiecDomowa(nowe.ssid, nowe.hasloWifi);
    bool prz = false;
    wifiProbowano = true;
    bool polaczona = a.siec.polacz(LIMIT_NOWEJ_SIECI_MS, [this]() {
      odnow();
      return false;
    }, prz);
    bool zapisane = false;
    std::string wynik;
    if (!polaczona) {
      wynik = "Nie łączę się z siecią " + nowe.ssid + ". Sprawdź hasło.";
    } else {
      bool ok = false;
      wynik = sprawdzAirBridgeP07(nowe, ok);
      if (ok) {
        ustawSekrety(nowe);
        zapisane = true;
        wynik = "Połączono z siecią " + nowe.ssid + ". " + wynik;
      } else {
        wynik = "Sieć " + nowe.ssid + " działa, ale: " + wynik;
      }
    }
    a.siec.wylacz();
    if (!zapisane) {
      a.siec.ustawSiecDomowa(stare.ssid, stare.hasloWifi);
      wynik += " Wracam do sieci " + stare.ssid + ".";
    }
    log("formularz: nowa sieć %s %s", nowe.ssid.c_str(), zapisane ? "zapisana" : "odrzucona, poprzednia zostaje");
    frm.stan = wynik;
    if (!zapisane) otworzFormularzP07();  // droga powrotu: ten sam formularz w poprzedniej sieci, nowy kod
    if (ekran() == Ekran::FORMULARZ) pokaz();
  }
  // W pętli czuwania: żądania telefonu; po 15 min albo 5 złych żądaniach powrót do U-01 (R-KONF-14, R-KONF-15).
  void obsluzFormularzP07() {
    if (!frm.serwer) return;
    if (!frm.sesja.aktywna(ms())) {
      log(frm.sesja.zamknietaZlymi() ? "formularz: 5 złych żądań (R-KONF-14)" : "formularz: minęło 15 min (R-KONF-15)");
      zamknijFormularzP07();
      if (ekran() == Ekran::FORMULARZ) {
        ustawEkran(Ekran::USTAWIENIA);
        r.ekranOd = teraz();
        pokaz();
      }
      return;
    }
    serwer::gniazdo()->obsluzZadania([this](const serwer::GniazdoHttp::Zadanie& z) {
      formularz::Odpowiedz fo =
          frm.sesja.obsluz(z.metoda, z.sciezka, z.cialo, ms(), z.metoda == "GET" ? stronaP07("") : std::string());
      if (fo.zapis) fo.cialo = stronaP07(zapiszFormularzP07(fo.pola));
      serwer::Odpowiedz o;
      o.kod = fo.kod;
      o.typ = fo.typ;
      o.cialo = fo.cialo;
      return o;
    });
    if (frm.sprawdzSiec) sprawdzSiecP07();
    if (frm.przerysuj && ekran() == Ekran::FORMULARZ) {
      frm.przerysuj = false;
      pokaz();
    }
  }
  void rysujFormularzP07() {
    DaneFormularzaQr d;
    d.bezWifi = frm.bezWifi;
    if (frm.serwer) {
      d.adresQr = frm.sesja.adresQr();
      d.adresReczny = frm.sesja.adresReczny();
      d.kodWazny = "Kod ważny do " + godzina(frm.koniecEp) + ".";
    }
    d.zapisano = frm.zapisano;
    d.stan = frm.stan;
    rysujFormularzQr(plotno(), d);
  }
  // odpowiedź telefonowi wyszła z handleClient(); chwila, żeby bufor TCP zdążył, zanim radio zgaśnie.
  void pauzaPrzedWylaczeniem() {
    for (uint32_t t = 0; t < PAUZA_PRZED_WYLACZENIEM_MS; t += 100) {
      odnow();
      czekaj(100);
    }
  }

  // ---------- U-02 sieć panelu, tylko ręcznie (5.6.5, R-KONF-07…R-KONF-11,
  // R-KONF-25…R-KONF-29, R-KONF-37) ----------
  // ◀ 2 s na U-01, U-05 albo na ekranie pierwszego uruchomienia: panel wyłącza połączenie z siecią domową, tworzy sieć
  // `cpap-eink-XXXX` z hasłem jednorazowym (esp_random przy włączonym radiu) i pokazuje U-02 z dwoma kodami QR. Serwer
  // i formularz te same co U-03 (token, 15 min, 5 złych żądań). Po zapisie sieci albo AirBridge: pauza, sieć panelu
  // wyłączona, 60 s na połączenie z nową siecią, `GET /api/status`, zapis w NVS tylko przy sukcesie; przy błędzie
  // poprzednie dane zostają i sieć panelu rusza od nowa z nowym hasłem i kodem. ◀ i 15 min: powrót tam, skąd
  // uruchomiono, sieć domowa bez zmian. Stan tylko w RAM: restart albo zanik zasilania = zwykły start, sieć panelu nie
  // startuje sama. W czasie sieci panelu panel nie pobiera danych (E-01 za tę przerwę nie powstaje).
  struct SiecPaneluP07 {
    bool aktywna = false, bezRadia = false, sprawdz = false, przerysuj = false;
    formularz::Sesja sesja;
    std::string nazwa, haslo, stan, zapisano;
    std::map<std::string, std::string> bledy;
    uint32_t koniecEp = 0;
    Ekran z = Ekran::USTAWIENIA;  // dokąd wraca ◀, upływ 15 min i błąd radia
    sekrety::Dane nowe;
  } ap;
  std::string losowyTekst(int n) {
    static const char A[] = "23456789abcdefghjkmnpqrstuvwxyz";  // 31 znaków, bez 0, 1, i, l, o
    std::string t;
    for (int i = 0; i < n; i++) t += A[a.siec.losowa() % (sizeof A - 1)];
    return t;
  }
  void otworzSiecPaneluP07(Ekran z, bool zachowajStan) {
    std::string stan = zachowajStan ? ap.stan : std::string();
    zamknijSiecPaneluP07();
    zamknijFormularzP07();
    ap = SiecPaneluP07();
    ap.z = z;
    ap.stan = stan;
    otaAktywne = false;  // okno serwisowe U-05 kończy wylacz() w adapterze
    wifiProbowano = true;
    serwer::GniazdoHttp* g = serwer::gniazdo();
    bool ok = g && a.siec.wlaczRadio();
    std::string token;
    if (ok) {  // T-WYM-35: hasło i token z generatora sprzętowego przy włączonym radiu
      ap.haslo = losowyTekst(ZNAKI_HASLA_AP);
      uint8_t bajty[serwer::BAJTY_TOKENU];
      for (int i = 0; i < serwer::BAJTY_TOKENU; i += 4) {
        uint32_t x = a.siec.losowa();
        for (int j = 0; j < 4; j++) bajty[i + j] = (uint8_t)(x >> (8 * j));
      }
      token = serwer::token(bajty);
      ap.nazwa = "cpap-eink-" + a.siec.idPanelu();
      ok = a.siec.uruchomSiecPanelu(ap.nazwa, ap.haslo) && serwer::adresPrywatny(a.siec.adresSieciPanelu()) &&
           g->start(80);
    }
    odnow();
    if (!ok) {
      if (g) g->stop();
      a.siec.wylacz();
      ap.bezRadia = true;
      ap.haslo.clear();
      ap.nazwa.clear();
      log("sieć panelu: nie mogę włączyć (radio albo serwer)");
      return;
    }
    ap.sesja.otworz(token, ms(), a.siec.adresSieciPanelu());
    ap.aktywna = true;
    ap.koniecEp = teraz() + serwer::CZAS_MS / 1000;
    log("sieć panelu: %s pod %s, formularz %s (hasło tylko na U-02)", ap.nazwa.c_str(),
        a.siec.adresSieciPanelu().c_str(), zegar() ? ("do " + godzina(ap.koniecEp)).c_str() : "na 15 min");
  }
  void zamknijSiecPaneluP07() {
    if (!ap.aktywna) return;
    if (serwer::gniazdo()) serwer::gniazdo()->stop();
    ap.aktywna = false;
    ap.sesja.zamknij();
    ap.haslo.clear();
    a.siec.wylacz();
    a.siec.ustawSiecDomowa(k.ssid, k.hasloWifi);  // R-KONF-29: zapisana sieć domowa przy najbliższym pobraniu
    odnow();
    log("sieć panelu: wyłączona, sieć domowa %s", k.ssid.empty() ? "brak" : k.ssid.c_str());
  }
  void wrocZSieciPaneluP07() {
    zamknijSiecPaneluP07();
    ustawEkran(ap.z);
    r.ekranOd = teraz();
    pokaz();
  }
  std::string stronaSieciPaneluP07(const std::string& komunikat) {
    formularz::Pola pf = formularz::zPliku(konfiguracjaPliku(), sekretyTeraz(), otoUzywalne(), k.pogodaLat, k.pogodaLon);
    terminyFormularzaB16(pf);
    return formularz::strona(pf, ap.sesja.sciezka(), komunikat, ap.bledy);
  }
  // Pola pliku konfiguracji z formularza (wspólne z U-03); zwraca dopisek przy błędzie zapisu.
  std::string zapiszPlikFormularzaP07(const formularz::Wynik& w) {
    std::string stara = konfiguracjaPliku(), nowa = formularz::scal(stara, w.plik);
    std::string staraMaska = ustawieniaB9().sprzet[EL_MASKA].wymieniono;  // R-MAS-19
    if (nowa == stara) {
      if (w.nowyModelMaski) nowyModelZFormularzaB16(true, staraMaska);  // R-MAS-16: data maski już dzisiejsza
      if (w.cofnijNowyModel) cofnijNowyModelZFormularzaB16();          // R-MAS-19
      return "";
    }
    bool ok = fsOk() && a.pliki.zapiszAtomowo("/konfiguracja.txt", nowa);
    if (w.nowyModelMaski) nowyModelZFormularzaB16(ok, staraMaska);  // R-MAS-16
    ustawieniaB9_ = parsujUstawieniaDnia(nowa);
    ustawieniaWczytane_ = true;
    b5.dzis.clear();
    log(ok ? "formularz: /konfiguracja.txt zapisany (R-KONF-17)" : "formularz: zapis /konfiguracja.txt nieudany");
    if (w.cofnijNowyModel) cofnijNowyModelZFormularzaB16();  // R-MAS-19, po zapisie pozostałych pól
    return ok ? "" : " Nie udało się zapisać w pamięci panelu.";
  }
  // POST w sieci panelu: pola pliku od razu; sieć i AirBridge dopiero po sprawdzeniu poza siecią panelu.
  std::string zapiszSiecPaneluP07(const std::map<std::string, std::string>& pola) {
    formularz::Wynik w = formularz::waliduj(pola, sekretyTeraz(), dzisiaj());
    ap.bledy = w.bledy;
    std::string dopisek = zapiszPlikFormularzaP07(w);
    ap.przerysuj = true;
    if (!w.ok()) return "Popraw pola oznaczone niżej; sieć panelu dalej działa." + dopisek;
    if (w.zmianaSieci || w.zmianaAirBridge || k.pierwszaKonfiguracja) {
      if (!sekrety::skonfigurowany(w.sekrety)) return "Wpisz nazwę sieci domowej i adres AirBridge." + dopisek;
      ap.sprawdz = true;
      ap.nowe = w.sekrety;
      ap.stan = "Łączę z siecią " + w.sekrety.ssid + "…";
      return "Panel wyłącza teraz swoją sieć i łączy się z siecią " + w.sekrety.ssid +
             ". Wynik zobaczysz na ekranie panelu. Telefon wróci do swojej sieci sam." + dopisek;
    }
    ap.zapisano = "Zapisano o " + godzina(teraz()) + ".";
    return ap.zapisano + dopisek;
  }
  // Po odpowiedzi telefonowi: sieć panelu wyłączona, nowa sieć 60 s (◀ przerywa), `GET /api/status`, zapis tylko przy
  // sukcesie (R-KONF-10, R-KONF-11, R-KONF-25…R-KONF-27, R-KONF-37).
  void sprawdzSiecPaneluP07() {
    ap.sprawdz = false;
    sekrety::Dane nowe = ap.nowe, stare = sekretyTeraz();
    Ekran z = ap.z;
    pauzaPrzedWylaczeniem();
    zamknijSiecPaneluP07();
    pokaz();  // U-02 `Łączę z siecią {nazwa}…`, bez kodów
    a.siec.ustawSiecDomowa(nowe.ssid, nowe.hasloWifi);
    PrzerwaniePrzyciskiem det;
    bool przerwane = false;
    bool polaczona = a.siec.polacz(LIMIT_NOWEJ_SIECI_MS, [this, &det]() {
      odnow();
      return det.krok(wcisniety() == Przycisk::LEWY);
    }, przerwane);
    if (przerwane) {  // ◀ w trakcie łączenia: anuluj, poprzednia sieć
      a.siec.wylacz();
      a.siec.ustawSiecDomowa(stare.ssid, stare.hasloWifi);
      log("sieć panelu: łączenie z %s przerwane ◀, poprzednia sieć zostaje", nowe.ssid.c_str());
      ap.stan.clear();
      czekajNaPuszczenie();
      ustawEkran(z);
      r.ekranOd = teraz();
      pokaz();
      return;
    }
    bool ok = false;
    std::string wynik;
    if (!polaczona) wynik = "Nie łączę się z siecią " + nowe.ssid + ". Sprawdź hasło. Poprzednie ustawienia zostają.";
    else wynik = sprawdzAirBridgeP07(nowe, ok);  // zapis w NVS tylko przy 200 i udanym odczycie kontrolnym
    if (ok) {
      ustawSekrety(nowe);
      k.pierwszaKonfiguracja = false;
      log("sieć panelu: nowa sieć %s zapisana, zwykła praca", nowe.ssid.c_str());
      ap = SiecPaneluP07();
      ustawEkran(Ekran::DZIEN);
      r.ekranOd = teraz();
      Pobranie p;
      p.pobierz = true;
      wykonajPobranie(p, false);  // 5.6.5 krok 4: pobranie i ekran dnia
      pokaz();
      return;
    }
    a.siec.wylacz();
    a.siec.ustawSiecDomowa(stare.ssid, stare.hasloWifi);
    log("sieć panelu: nowa sieć %s odrzucona, poprzednia zostaje", nowe.ssid.c_str());
    ap.stan = wynik;
    otworzSiecPaneluP07(z, true);  // nowe hasło i nowy kod (R-KONF-08)
    ustawEkran(Ekran::SIEC_PANELU);
    r.ekranOd = teraz();
    pokaz();
  }
  // W pętli czuwania: żądania telefonu; 15 min albo 5 złych żądań → powrót (R-KONF-29). Noc bez wyjątku: po 15 min
  // panel też wraca (mapa 5.6.2, „W nocy”).
  void obsluzSiecPaneluP07() {
    if (!ap.aktywna) return;
    if (!ap.sesja.aktywna(ms())) {
      log(ap.sesja.zamknietaZlymi() ? "sieć panelu: 5 złych żądań (R-KONF-14)" : "sieć panelu: minęło 15 min (R-KONF-29)");
      ap.stan.clear();
      if (ekran() == Ekran::SIEC_PANELU) wrocZSieciPaneluP07();
      else zamknijSiecPaneluP07();
      return;
    }
    serwer::gniazdo()->obsluzZadania([this](const serwer::GniazdoHttp::Zadanie& z) {
      formularz::Odpowiedz fo = ap.sesja.obsluz(z.metoda, z.sciezka, z.cialo, ms(),
                                                z.metoda == "GET" ? stronaSieciPaneluP07("") : std::string());
      if (fo.zapis) fo.cialo = stronaSieciPaneluP07(zapiszSiecPaneluP07(fo.pola));
      serwer::Odpowiedz o;
      o.kod = fo.kod;
      o.typ = fo.typ;
      o.cialo = fo.cialo;
      return o;
    });
    odnow();
    if (ap.sprawdz) sprawdzSiecPaneluP07();
    else if (ap.przerysuj && ekran() == Ekran::SIEC_PANELU) {
      ap.przerysuj = false;
      pokaz();
    }
  }
  void rysujSiecPaneluP07() {
    DaneSieciPanelu d;
    if (ap.aktywna) {
      d.nazwa = ap.nazwa;
      d.haslo = ap.haslo;  // tylko tu i w kodzie QR (R-KONF-08)
      d.adresQr = ap.sesja.adresQr();
      d.adresReczny = ap.sesja.adresReczny();
      d.wazne = zegar() ? "Ważne do " + godzina(ap.koniecEp) + "." : "Ważne 15 minut.";
      if (!ap.zapisano.empty()) d.wazne += " " + ap.zapisano;
    }
    d.stan = ap.stan;
    d.bezRadia = ap.bezRadia && !ap.aktywna;
    d.instrukcja = ekran() == Ekran::DZIEN;  // pierwsze uruchomienie bez sieci (R-KONF-07)
    rysujSiecPanelu(plotno(), d);
  }

  // Ekrany spraw otwierane przyciskiem (pokaz()); false = brak danych, rysuj ekran dnia.
  bool pokazB5(const std::string& dzis) {
    if (!silnikDzis(dzis)) return false;
    DaneDnia d = daneDnia(dzis);
    switch (ekran()) {
      case Ekran::OMOWIONE: {
        const sprawy::SprawaL* s = sprawaB5(b5.kluczEkranu);
        if (!s) return false;
        rysujOmowione(plotno(), trescOmowione(*s, dzis, wariantOmowieniaB11(*s, dzis)));
        return true;
      }
      case Ekran::RAPORT: {
        const sprawy::SprawaL* s = sprawaB5(b5.kluczEkranu);
        if (!s) return false;
        rysujRaport(plotno(), trescRaportu(*s, d));
        // R-LEK-26: raport tematyczny otwarty z ekranu L po potwierdzeniu odblokowuje następne „Omówione”
        if (!s->kluczWzorca.empty()) {  // zapis pliku reguł tylko przy zmianie
          std::string przed = b5.pamiec[s->kluczWzorca].stanWzorca;
          wzorce::raportZEkranuL(b5.pamiec, s->kluczWzorca, dzis);
          if (b5.pamiec[s->kluczWzorca].stanWzorca != przed) zapiszPamiecRegul();
        }
        log("raport dla lekarza: brak modułu raportu (%s)", KOD_RAPORTU_BRAK);
        return true;
      }
      case Ekran::SPRAWY: {
        std::vector<sprawy::SprawaMetryki> m;
        reguly::Odpowiedzi o;
        sprawy::WejscieSpraw ws = wejscieSprawB5(dzis, m, o);
        std::vector<sprawy::KartaSprawy> ks = sprawy::kartySpraw(ws);
        b5.karta = przytnijKarte(b5.karta, (int)ks.size());
        DaneSpraw ds = trescSpraw(ks, b5.karta, ws);
        ds.liczby = profilB18();  // R-KOK-22: ◀ z pierwszej karty do „Co znaczą te liczby”
        rysujSprawy(plotno(), ds);
        return true;
      }
      case Ekran::PRZENIESIONE: {
        const sprawy::SprawaL* s = sprawaWiemB5(dzis);
        if (!s) return false;
        rysujPotwierdzenie(plotno(), trescPrzeniesione(*s, d, cofniecieB5()));
        return true;
      }
      case Ekran::ZAPISANO: {
        const sprawy::Odpowiedz* o = nullptr;
        for (auto& x : odpowiedzi)
          if (x.odp != 'b') o = &x;
        if (!b5.ostatniaOdp.data.empty()) o = &b5.ostatniaOdp;
        if (!o) return false;
        rysujPotwierdzenie(plotno(), trescZapisano(o->pytanie, o->odp == 't', d, cofniecieB5()));
        return true;
      }
      default: return false;
    }
  }
  // Akcje ekranów spraw z mapy przycisków (obsluz()).
  void akcjaB5(Akcja ak, Ekran nowy, const std::string& dzis) {
    switch (ak) {
      case Akcja::WIEM:
        if (!b5.kluczEkranu.empty()) {
          const sprawy::SprawaL* sw = sprawaB5(b5.kluczEkranu);
          sprawy::zapiszWiem(b5.pamiec, b5.kluczEkranu, dzis, sw ? sw->pilnosc : reguly::Pilnosc::BRAK);
          b5.kluczWiem = b5.kluczEkranu;
          zapiszPamiecRegul();
          log("„wiem”: %s", b5.kluczEkranu.c_str());
        }
        break;
      case Akcja::COFNIJ_WIEM:
        if (const sprawy::SprawaL* s = sprawaWiemB5(dzis)) {
          sprawy::cofnijWiem(b5.pamiec, s->klucz);
          zapiszPamiecRegul();
          log("„wiem” cofnięte: %s", s->klucz.c_str());
        }
        break;
      case Akcja::OMOWIONE_TAK:
        if (!b5.kluczEkranu.empty()) {
          const sprawy::SprawaL* so = sprawaB5(b5.kluczEkranu);
          // (5.12.7, R-LEK-24…27): przez stan wzorca, który liczy potwierdzenia A i blokadę R-LEK-26; zapis pola
          // „Omówione” pod kluczem sprawy, gdy różni się od klucza wzorca (gałąź bez własnego wyjścia, np. L-08 szybki)
          bool zapisane = false;
          if (so && !so->kluczWzorca.empty()) {
            zapisane = wzorce::omowione(b5.pamiec, so->kluczWzorca, dzis, zestawB11());
            if (zapisane && so->kluczWzorca != so->klucz) sprawy::zapiszOmowione(b5.pamiec, so->klucz, dzis, so->pilnosc);
          } else {
            sprawy::zapiszOmowione(b5.pamiec, b5.kluczEkranu, dzis, so ? so->pilnosc : reguly::Pilnosc::BRAK);
            zapisane = true;
          }
          if (zapisane) zapiszPamiecRegul();
          log("„Omówione z lekarzem”: %s%s", b5.kluczEkranu.c_str(), zapisane ? "" : " bez zapisu (najpierw nowy raport, R-LEK-26)");
        }
        break;
      case Akcja::SPRAWY_OTWORZ: zapiszKarteB5(1); break;
      case Akcja::SPRAWY_OD_KONCA: zapiszKarteB5(std::max(1, kartySprawB5(dzis))); break;  // ◀ z U-01 (R-KOK-20)
      case Akcja::SPRAWY_DALEJ: zapiszKarteB5(b5.karta + 1); break;
      case Akcja::SPRAWY_WSTECZ: zapiszKarteB5(std::max(1, b5.karta - 1)); break;
      case Akcja::ODP_TAK:
      case Akcja::ODP_NIE:
      case Akcja::ODP_POMIN:
        if (b5.pytanie.jest) {
          sprawy::Odpowiedz o{b5.pytanie.data, b5.pytanie.pytanie,
                              ak == Akcja::ODP_TAK ? 't' : ak == Akcja::ODP_NIE ? 'n' : 'b'};
          sprawy::zapiszOdpowiedz(odpowiedzi, o);
          zapiszOdpowiedzi();
          a.nvs.ustawTekst("cpap", "pyt_tn", "");
          if (ak != Akcja::ODP_POMIN) b5.ostatniaOdp = o;
          b5.zdjeteWyjazd.clear();
          if (!a.nvs.tekst("cpap", "wyj_cof", "").empty()) a.nvs.ustawTekst("cpap", "wyj_cof", "");
          log("pytanie %s %s: %s", o.data.c_str(), sprawy::nazwaPytania(o.pytanie), sprawy::wierszOdpowiedzi(o).c_str());
          if (o.pytanie == sprawy::Pytanie::WYJAZD && o.odp == 'n') {  // R-CZY-23: zdjęcie oznaczenia A
            for (auto& ow : okresyWyjazdu(czynniki))
              if (ow.od == o.data) b5.zdjeteWyjazd = zdejmijPozaDomem(czynniki, ow);
            zapiszCzynniki();
            // cofnięcie ◀ 2 s w 60 s (R-PRZ-11) działa także po uśpieniu: odpowiedź i zdjęte wpisy w NVS
            std::string z = o.data;
            for (auto& x : b5.zdjeteWyjazd) z += "|" + zapiszWpis(x);
            a.nvs.ustawTekst("cpap", "wyj_cof", z);
            log("czynniki: „nie” na R-CZY-22, zdjęte oznaczenie z %d nocy", (int)b5.zdjeteWyjazd.size());
          }
        }
        break;
      case Akcja::COFNIJ_ODP:
        if (b5.ostatniaOdp.data.empty()) wczytajCofniecieWyjazduB15();  // po uśpieniu
        if (!b5.ostatniaOdp.data.empty()) {
          sprawy::cofnijOdpowiedz(odpowiedzi, b5.ostatniaOdp.data, b5.ostatniaOdp.pytanie);
          zapiszOdpowiedzi();
          if (b5.ostatniaOdp.pytanie == sprawy::Pytanie::WYJAZD && !b5.zdjeteWyjazd.empty()) {  // R-CZY-23, R-PRZ-11
            przywrocWpisy(czynniki, b5.zdjeteWyjazd);
            b5.zdjeteWyjazd.clear();
            zapiszCzynniki();
          }
          if (!a.nvs.tekst("cpap", "wyj_cof", "").empty()) a.nvs.ustawTekst("cpap", "wyj_cof", "");
          log("odpowiedź cofnięta: %s", sprawy::wierszOdpowiedzi(b5.ostatniaOdp).c_str());
          b5.ostatniaOdp = sprawy::Odpowiedz();
        }
        break;
      default: return;
    }
    (void)nowy;
    b5.dzis.clear();  // stan spraw i kolejka od nowa przy następnym rysowaniu
  }

  void rysujPytanieTeraz() {
    DanePytania d;
    d.dataPoranka = r.pyt.data;
    d.innaNoc = r.pyt.innaNoc;
    d.kursor = r.pyt.kursor;
    d.zaznaczone = r.pyt.zaznaczone;
    d.termin = r.pyt.termin;
    const Noc* n = nocZDnia(hist, r.pyt.data);
    d.opisNocy = n ? opisNocyPytania(*n, norma(r.pyt.data), r.pyt.innaNoc) : "";
    rysujPytanie(plotno(), d);
  }

  std::string kiedyPobranoOto() {
    std::string d;
    int m;
    dataIMinuta(r.oto.pobrano, d, m);
    return kiedyOdczyt(d, m, dzisiaj());
  }

  // Wiersze diagnostyki U-05 z pomiaru baterii (napięcie i trend, średni czas Wi-Fi i aktywności z doby).
  void liniePomiaruBaterii(std::string& linia, std::string& liniaWifi) {
    if (!baterieWczytane) wczytajBaterie();
    linia = liniaBaterii(trendBaterii(baterie), bateria);
    std::string sr = sredniaDoby(baterie, zegar() ? teraz() : 0);
    if (!sr.empty()) liniaWifi += " · " + sr.substr(6);
  }

  // minimum sterty i stosu z doby (pomiar z tego wybudzenia razem z zapisanymi w RTC).
  PamiecWybudzenia pamiecTeraz() {
    PamiecWybudzenia p{a.diagnostyka.minWolnaSterta(), a.diagnostyka.zapasStosu()};
    return p;
  }
  int32_t dobaPamieci() {
    std::string d = dzisiaj();
    return d.empty() ? 0 : (int32_t)nrDnia(d) + 1;
  }
  void zapamietajPamiec(const PamiecWybudzenia& p) {
    int32_t doba = dobaPamieci();
    if (doba != r.pamiecDoba) {
      r.pamiecDoba = doba;
      r.pamiecSterta = p.minSterta;
      r.pamiecStos = p.zapasStosu;
      return;
    }
    r.pamiecSterta = std::min(r.pamiecSterta, p.minSterta);
    r.pamiecStos = std::min(r.pamiecStos, p.zapasStosu);
  }
  // Dopisek do linii ekranu testu: `min. sterta 120 KB, stos 9 KB` (z ` poza budżetem`, gdy poniżej progów).
  std::string tekstPamieci() {
    PamiecWybudzenia p = pamiecTeraz();
    if (r.pamiecDoba && r.pamiecDoba == dobaPamieci()) {
      p.minSterta = std::min(p.minSterta, r.pamiecSterta);
      p.zapasStosu = std::min(p.zapasStosu, r.pamiecStos);
    }
    char b[96];
    snprintf(b, sizeof b, "min. sterta %lu KB, stos %lu KB%s", (unsigned long)(p.minSterta / 1024),
             (unsigned long)(p.zapasStosu / 1024), pamiecWBudzecie(p) ? "" : " poza budżetem");
    return b;
  }

  // linia stanu danych `Dane: …` (T-WYM-39) w diagnostyce U-05 (log portu): problemy (układ partycji,
  // pamięć plików, przywracanie, migracja, dane z nowszej wersji) po kolei, wszystkie, a bez problemów wersja plików
  // (8.9 krok 6: z dopiskiem `przywrócono {n} z {m}`).
  std::string liniaDanych() {
    wczytajOdrzucone();  // lista odrzuconych przy każdym rysowaniu U-05 (jeden odczyt NVS)
    std::vector<std::string> cz;
    if (staryUklad) cz.push_back("układ partycji: stary, wgraj kablem");  // R7 karty 
    if (!fsOk()) cz.push_back("pamięć plików: nie zamontowana");
    if (przywrocenie.proba && !przywrocenie.ok)
      cz.push_back("przywracanie: niezgodna liczba (" + std::to_string(przywrocenie.przywrocone) + " z " +
                   std::to_string(przywrocenie.oczekiwane) + ")");
    // U2 wariant A: pliki zrzutu odrzucone w oknie; okno wraca przy następnym starcie (na U-05 liczba plików)
    if (odrzuconeWczytane && !odrzucone.empty()) cz.push_back("przywracanie: odrzucone " + listaOdrzuconych());
    for (auto& b : bledyMigracji) cz.push_back(b);
    for (auto& f : mag.plikiTylkoDoOdczytu()) cz.push_back("dane z nowszej wersji: " + nazwaPliku(f));
    for (auto& p : nvsNowsze) cz.push_back("dane z nowszej wersji: NVS " + p);
    if (cz.empty()) {
      std::string v;
      for (int x : wersjePlikow) v += (v.empty() ? "" : ", ") + std::to_string(x);
      cz.push_back(v.empty() ? "pliki bez danych" : (wersjePlikow.size() > 1 ? "wersje plików " : "wersja plików ") + v);
      if (przywrocenie.ok)  // 8.9 krok 6: wynik przywrócenia obok wersji plików (liczba nocy w linii historii)
        cz.push_back("przywrócono " + std::to_string(przywrocenie.przywrocone) + " z " +
                     std::to_string(przywrocenie.oczekiwane));
    }
    std::string l = "Dane: " + cz[0];
    for (size_t i = 1; i < cz.size(); i++) l += " · " + cz[i];
    return l;
  }

  // ---------- U-05 „Test sprzętu” według kanwy (T-WYM-39, R-KONF-41) ----------
  // `dd.mm hh:mm` chwili; "" bez zegara.
  std::string chwila(uint32_t ep) {
    std::string d = dataZ(ep);
    return d.empty() ? "" : ddmm(d) + " " + godzina(ep);
  }
  // `hh:mm` dziś, inaczej `dd.mm hh:mm` (godzina w nawiasie przy ostatnim połączeniu Wi-Fi, R-KONF-41).
  std::string kiedyChwila(uint32_t ep) {
    std::string d = dataZ(ep);
    return d.empty() ? "" : d == dzisiaj() ? godzina(ep) : ddmm(d) + " " + godzina(ep);
  }
  // Ostatnie połączenie z Wi-Fi w RTC (R-KONF-41: bez okna serwisowego U-05 pokazuje je z godziną w nawiasie).
  void zapamietajWifi() {
    if (!a.siec.polaczona()) return;
    kopiuj(r.wifiIp, sizeof r.wifiIp, a.siec.adresIp());
    kopiuj(r.wifiSsid, sizeof r.wifiSsid, a.siec.nazwaSieci());
    r.wifiRssi = (int16_t)a.siec.rssi();
    r.wifiEp = zegar() ? teraz() : 0;
  }
  // `−61 dBm` (minus U+2212 jak na kanwie)
  static std::string tekstRssi(int v) { return (v < 0 ? "−" + std::to_string(-v) : std::to_string(v)) + " dBm"; }

  // Wartość wiersza `Pamięć panelu`: najpierw problem (pierwszy z linii `Dane: …`, ta sama kolejność, w wariancie,
  // który mieści się w kolumnie; pełny tekst w diagnostyce), po przywróceniu wynik, inaczej `wolne {n} MB`.
  std::string wartoscPamieci() {
    const std::string n = "Pamięć panelu";
    if (staryUklad) return dopasujWartosc(n, {"stary układ: wgraj kablem", "układ: wgraj kablem"});
    if (!fsOk()) return "nie zamontowana";
    wczytajOdrzucone();
    std::string p = std::to_string(przywrocenie.przywrocone), o = std::to_string(przywrocenie.oczekiwane);
    if (przywrocenie.proba && !przywrocenie.ok)
      return dopasujWartosc(n, {"niezgodna liczba (" + p + " z " + o + ")", "niezgodna liczba nocy", "niezgodna liczba"});
    // U2 wariant A: pliki zrzutu odrzucone w oknie; pełna lista w `Dane: … przywracanie: odrzucone {pliki}`
    // (log portu), na U-05 liczba plików (nazwy nie mieszczą się w kolumnie), integracja 
    if (odrzuconeWczytane && !odrzucone.empty()) {
      size_t ile = odrzucone.size();
      std::string pl = ile == 1 ? "plik" : (ile % 10 >= 2 && ile % 10 <= 4 && (ile % 100 < 12 || ile % 100 > 14))
                                                ? "pliki" : "plików";
      return dopasujWartosc(n, {"odrzucone " + std::to_string(ile) + " " + pl, "odrzucone pliki"});
    }
    auto bezCsv = [](std::string f) {
      if (f.size() > 4 && f.compare(f.size() - 4, 4, ".csv") == 0) f.erase(f.size() - 4);
      return f;
    };
    if (!bledyMigracji.empty()) {  // `migracja {plik}: błąd`
      const std::string& b = bledyMigracji.front();
      size_t k0 = std::string("migracja ").size(), k1 = b.find(": ", k0);
      std::string plik = k1 == std::string::npos ? "" : b.substr(k0, k1 - k0);
      return dopasujWartosc(n, {b, "migracja " + bezCsv(plik) + ": błąd", "błąd migracji"});
    }
    std::vector<std::string> nowsze = mag.plikiTylkoDoOdczytu();
    if (!nowsze.empty()) {
      std::string f = nazwaPliku(nowsze.front());
      return dopasujWartosc(n, {"dane z nowszej wersji: " + f, "dane z nowszej wersji"});  // plik w diagnostyce
    }
    if (!nvsNowsze.empty())
      return dopasujWartosc(n, {"dane z nowszej wersji: NVS " + nvsNowsze.front(), "dane z nowszej wersji"});
    if (przywrocenie.ok)
      return dopasujWartosc(n, {"przywrócono " + p + " z " + o, "przywrócono " + p + "/" + o, p + "/" + o + " nocy"});
    long long w = a.pliki.wolneMiejsce();
    return w < 0 ? "nie zamontowana" : "wolne " + std::to_string(w / (1024LL * 1024LL)) + " MB";
  }

  // Wartość wiersza `AirBridge` (R-KONF-41): adres z konfiguracji z wersją z ostatniej odpowiedzi `/api/status`;
  // bez odpowiedzi w tym wybudzeniu `brak odpowiedzi od {dd.mm hh:mm}` (ostatnia udana), bez żadnej `brak odpowiedzi`.
  std::string wartoscAirBridge() {
    const std::string n = "AirBridge";
    bool brakTeraz = httpKod < 0;  // 0 = bez zapytania w tym wybudzeniu; kod HTTP > 0 = AirBridge odpowiedział
    if (!r.ab.ma || (brakTeraz && chwila(r.ab.pobrano).empty())) return "brak odpowiedzi";
    if (brakTeraz) {
      std::string c = chwila(r.ab.pobrano);
      return dopasujWartosc(n, {"brak odpowiedzi od " + c, "brak odp. od " + c});
    }
    if (!r.ab.wersja[0])
      return dopasujWartosc(n, {k.abAdres + " · wersja nieznana", k.abAdres + " · bez wersji", k.abAdres});
    // `version` AirBridge ma już `v` (np. `v2.2.1`, z gita `v2.2.1-25-gebbd145`); za długa: bez części po `-`
    std::string v = r.ab.wersja;
    if (v[0] != 'v') v = "v" + v;
    return dopasujWartosc(n, {k.abAdres + " · " + v, k.abAdres + " · " + v.substr(0, v.find('-'))});
  }

  // Chwila ostatniego udanego pobrania nocy (T-WYM-39, D52 karty): najpóźniejsza z chwili zapisanej w NVS przy
  // pobraniu kompletnej nocy z AirBridge i z `import` w rekordach nocy (R1); po przywróceniu zrzutu noce nie mają
  // `import`, a chwila w NVS zostaje. "–" bez obu.
  std::string ostatniePobranie() {
    uint32_t m = a.nvs.liczbaU("cpap", NVS_KLUCZ_POBRANIE_NOCY, 0);
    for (auto& n : hist) m = std::max(m, n.import);
    std::string c = m ? chwila(m) : "";
    return c.empty() ? "–" : c;
  }

  // `Oprogramowanie`: wersja i slot, z dopiskiem `· jest wersja {X}` albo `· odrzucona {X}` (aktualizacja, R-NF-35).
  std::string wartoscOprogramowania() {
    wczytajAkt();
    std::string w = wersjaWydania(a.uruchomienie.wersja()), sl = " (" + a.uruchomienie.slot() + ")";
    std::string d = aktualizacja::dopisekU05(akt);
    if (d.empty()) return w + sl;
    // kolumna U-05 ma ok. 188 px: krótsze warianty dopisku, pełny tekst w diagnostyce (`Wersja: …`)
    std::string kr = d;
    size_t q = kr.find("jest wersja ");
    if (q != std::string::npos) kr.replace(q, 12, "jest ");
    q = kr.find("odrzucona ");
    if (q != std::string::npos) kr.replace(q, 10, "odrz. ");
    return dopasujWartosc("Oprogramowanie", {w + sl + d, w + d, w + kr, kr.substr(3), w + d});
  }
  // Wiersze U-05 według kanwy (`design/kanwa/U05-test-sprzetu.dc.html`), w tej samej kolejności.
  DaneTestu daneTestu() {
    zapamietajWifi();
    DaneTestu d;
    bool wifi = a.siec.polaczona();
    std::string czasWifi = r.wifiEp ? kiedyChwila(r.wifiEp) : "";
    std::string poCzasie = czasWifi.empty() ? "" : " (" + czasWifi + ")";
    std::string panel = wifi ? a.siec.adresIp() : r.wifiIp[0] ? std::string(r.wifiIp) + poCzasie : "brak połączenia z Wi-Fi";
    std::string siec = wifi ? a.siec.nazwaSieci() + " " + tekstRssi(a.siec.rssi())
                       : r.wifiIp[0] ? std::string(r.wifiSsid) + " " + tekstRssi(r.wifiRssi) + poCzasie : "–";
    std::vector<std::string> oto = wartosciTestuOtoczenia(r.oto, kiedyPobranoOto());
    d.lewa = {
        {"Oprogramowanie", wartoscOprogramowania()},  // ADR-0032 pkt 2, R-NF-35: dopisek aktualizacji
        {"Zestaw reguł", "wbudowany"},  // zestawu danych reguł w pamięci panelu jeszcze nie ma
        {"Pamięć panelu", wartoscPamieci()},
        {"Karta", "brak karty"},        // sterownik karty microSD: (R-AW-29)
        {"Źródło otoczenia", dopasujWartosc("Źródło otoczenia", oto)},
    };
    d.prawa = {
        {"Panel", dopasujWartosc("Panel", {panel})},
        {"Wi-Fi", dopasujWartosc("Wi-Fi", {siec})},
        {"AirBridge", wartoscAirBridge()},
        {"Ostatnie pobranie", ostatniePobranie()},
        // M-25 (R-KONF-41), krzywa; bez odczytu napięcia w tym wybudzeniu `–` (E-05 bez zmian)
        {"Bateria", bateriaZOdczytu && procentBat >= 0 ? std::to_string(procentBat) + "%" : "–"},
    };
    // tryb porównawczy (ADR-0036 „Zmiana 2026-11-09”): ostatnie 7 dób z porównaniem nocy z plików z raportem JSON;
    // przy 7 z 7 zgodnych dopisek „można przełączyć”, przełączenie kluczem `noc_zrodlo=plik` albo usunięciem wiersza
    // (od 11.11.2026 domyślnie `plik`, ADR-0036 „Zmiana 2026-11-11”); w trybie `plik` wiersz `włączone (plik)`
    d.dol = {{"Noce z plików", dopasujWartosc("Noce z plików",
                                              porownanie::wartosciU05(porownanie::policz(wpisyPorownania()), zrodloNocy()),
                                              TEST_SZEROKOSC)}};
    d.ramkaLewo = ramkaOkna();
    d.ramkaPrawo = "hasła i dane terapii nie są pokazywane";
    return d;
  }

  // Diagnostyka prototypu, której kanwa U-05 nie ma (wybudzenie, Wi-Fi, AirBridge, historia, czynniki, czujnik,
  // bateria w V, otoczenie, sterta i stos, `Dane: …`): jeden blok wierszy w logu portu przy każdym rysowaniu U-05.
  std::vector<std::string> diagnostykaTestu() {
    std::string wifi = a.siec.polaczona() ? a.siec.opisPolaczenia() : "brak: " + wifiOpis;
    std::string liniaWifi = "Wi-Fi: " + wifi, liniaBat;
    liniePomiaruBaterii(liniaBat, liniaWifi);
    return {
        "Wersja: " + std::string(a.uruchomienie.wersja()) + " · slot " + a.uruchomienie.slot() +
            aktualizacja::dopisekU05(akt),
        "Wybudzenie: " + powod + " · licznik " + std::to_string(r.wybudzen) + " · przycisk " + ostatniPrzycisk,
        liniaWifi,
        "AirBridge: " + abAdres + " · HTTP " + std::to_string(httpKod) +
            (httpKod == 401              ? " logowanie odrzucone (konto albo hasło AirBridge)"
             : httpKod == AB_KOD_URWANE  ? " (odpowiedź urwana)"
             : httpKod == AB_KOD_ZA_DUZO ? " (odpowiedź za duża)"
                                         : "") +
            " · " +
            std::to_string(httpMs) + " ms",
        std::string("AirBridge ") + (r.ab.wersja[0] ? r.ab.wersja : "?") + " · tryb " + (r.ab.tryb[0] ? r.ab.tryb : "?") +
            " · rop " + std::to_string(r.ab.rop) + (r.ab.wBudowie ? " · noc w budowie" : "") +
            (r.ab.zalegle > 0 ? " · zaległe " + std::to_string(r.ab.zalegle) : "") +
            (r.ab.czekajace > 0 ? " · czeka " + std::to_string(r.ab.czekajace) : ""),
        "Ostatni odczyt: " + (r.ab.pobrano ? godzina(r.ab.pobrano) : std::string("–")) + " · historia " +
            std::to_string(hist.size()) + " nocy" +
            (importWczytane >= 0
                 ? " · import +" + std::to_string(importWczytane) + " / pominięte " + std::to_string(importPominiete)
                 : std::string("")),
        "Czynniki: " + std::to_string(czynniki.size()) + " nocy · ekrany: " + std::to_string(dziennik.size()) +
            " · czujnik panelu " + przecinek1(temp) + "° · " + calkowita(wilg) + "%",
        liniaBat,
        liniaTestuOtoczenia(r.oto, zegar() ? teraz() : 0, kiedyPobranoOto()),
        "Pamięć: " + tekstPamieci(),
        liniaDanych(),
        porownanie::liniaDiagnostyki(wpisyPorownania(), zrodloNocy()),
    };
  }

  // Wiersze U-05 jako tekst (TekstyWybudzenia::linieTestu): `{nazwa}: {wartość}` i obie części ramki.
  static std::vector<std::string> tekstyTestu(const DaneTestu& d) {
    std::vector<std::string> l;
    for (auto* k : {&d.lewa, &d.prawa, &d.dol})
      for (auto& w : *k) l.push_back(w.nazwa + ": " + w.wartosc);
    l.push_back(d.ramkaLewo);
    l.push_back(d.ramkaPrawo);
    return l;
  }

  void rysujTestTeraz() {
    DaneTestu d = daneTestu();
    std::vector<std::string> l = tekstyTestu(d), g = diagnostykaTestu();
    tk.linieTestu.insert(tk.linieTestu.end(), l.begin(), l.end());
    tk.diagnostykaTestu.insert(tk.diagnostykaTestu.end(), g.begin(), g.end());
    for (auto& w : g) log("U-05 · %s", w.c_str());
    rysujTest(plotno(), d);
  }

  // Ramka okna serwisowego U-05 (lewa część; R-KONF-36, R-NF-29, T-WYM-45, T-WYM-87).
  std::string ramkaOkna() {
    if (oknoPrzywr) return tekstOknaPrzywracania();  // T-WYM-87
    if (!k.oknoZHaslem) return "Wgrywanie przez Wi-Fi: wyłączone";
    if (!otaAktywne) return "Wgrywanie przez Wi-Fi: brak sieci";
    long zostalo = (long)(int32_t)(otaDo - ms());
    if (zostalo <= 0) return "Wgrywanie przez Wi-Fi: zamknięte";
    if (zegar()) return "Wgrywanie przez Wi-Fi do " + godzina(teraz() + (uint32_t)(zostalo / 1000));
    return "Wgrywanie przez Wi-Fi: " + std::to_string((zostalo + 59999) / 60000) + " min";  // bez zegara jak
  }

  // T-WYM-87: `Okno przywracania do {hh:mm}` przy znanym zegarze, bez zegara `Okno przywracania: {m} min`
  // (pełne minuty do końca okna, zaokrąglone w górę); lewa część ramki U-05.
  std::string tekstOknaPrzywracania() {
    long zostalo = (long)(int32_t)(oknoPrzywrDoMs - ms());
    if (zostalo < 0) zostalo = 0;
    if (zegar()) return "Okno przywracania do " + godzina(teraz() + (uint32_t)(zostalo / 1000));
    return "Okno przywracania: " + std::to_string((zostalo + 59999) / 60000) + " min";
  }

  // ---------- okno przywracania i komenda `W` (T-WYM-87, R8 karty) ----------
  // Treść komendy `W`: dokładnie `n` bajtów z portu; false, gdy przez LIMIT_BAJTOW_W_MS nie przyszedł kolejny bajt albo
  // minęła chwila `doMs` (U9: limit całej treści). Bufor rośnie kawałkami KAWALEK_TRESCI_W w miarę
  // przychodzenia bajtów (bez podwajania, U11), a nie od razu o zadeklarowany rozmiar: zakłócony nagłówek nie żąda
  // 4 MB sterty, zanim przyjdzie choć bajt treści.
  bool czytajTresc(size_t n, std::string& t, uint32_t doMs) {
    t.clear();
    t.reserve(std::min(n, KAWALEK_TRESCI_W));
    uint32_t ost = ms();
    while (t.size() < n) {
      if ((int32_t)(ms() - doMs) >= 0) return false;
      int c = a.port.znak();
      if (c >= 0) {  // limit liczony od ostatniego bajtu (albo od początku czekania)
        if (t.size() == t.capacity()) t.reserve(std::min(n, t.size() + KAWALEK_TRESCI_W));
        t.push_back((char)c);
        ost = ms();
        if ((t.size() & 1023u) == 0) odnow();
        continue;
      }
      odnow();
      if (ms() - ost >= LIMIT_BAJTOW_W_MS) return false;
      czekaj(1);
    }
    return true;
  }
  // `n` bajtów treści z portu bez zapamiętywania (plik obrazków ponad limit); false jak czytajTresc().
  bool wyrzucTresc(size_t n, uint32_t doMs) {
    uint32_t ost = ms();
    size_t ile = 0;
    while (ile < n) {
      if ((int32_t)(ms() - doMs) >= 0) return false;
      if (a.port.znak() >= 0) {
        if ((++ile & 1023u) == 0) odnow();
        ost = ms();
        continue;
      }
      odnow();
      if (ms() - ost >= LIMIT_BAJTOW_W_MS) return false;
      czekaj(1);
    }
    return true;
  }
  // po zerwanej treści komendy `W` bajty z portu wyrzucane do LIMIT_BAJTOW_W_MS ciszy (najwyżej
  // MAKS_PLIKU_W bajtów i najwyżej do `doMs`), żeby reszta treści nie trafiła do pętli jako komendy (`Z`, `O`, `K`).
  void doCiszy() { doCiszy(ms() + LIMIT_BAJTOW_W_MS + (uint32_t)(MAKS_PLIKU_W / 1024) * LIMIT_W_NA_KB_MS); }
  void doCiszy(uint32_t doMs) {
    uint32_t ost = ms();
    size_t ile = 0;
    while (ms() - ost < LIMIT_BAJTOW_W_MS && ile < MAKS_PLIKU_W && (int32_t)(ms() - doMs) < 0) {
      if (a.port.znak() >= 0) {
        if ((++ile & 1023u) == 0) odnow();
        ost = ms();
        continue;
      }
      odnow();
      czekaj(1);
    }
    if (ile) log("komenda W: %lu B reszty treści wyrzucone do ciszy", (unsigned long)ile);
  }
  // Reszta wiersza komendy po pierwszym znaku (do `\n`, najwyżej MAKS_WIERSZA_KOMENDY znaków, limit 5 s).
  std::string resztaWiersza() {
    std::string w;
    uint32_t ost = ms();
    for (;;) {
      int c = a.port.znak();
      if (c == '\n') break;
      if (c >= 0) {
        if (c != '\r' && w.size() < MAKS_WIERSZA_KOMENDY) w += (char)c;
        ost = ms();
        continue;
      }
      odnow();
      if (ms() - ost >= LIMIT_BAJTOW_W_MS) break;
      czekaj(1);
    }
    return w;
  }
  // Odpowiedź na komendę `W`; w oknie przywracania plik zrzutu odrzucony trafia na listę `odrzucone`, przyjęty z niej
  // schodzi (U2 wariant A: znacznik udanego przywrócenia tylko bez odrzuconych plików).
  void wynikW(const std::string& nazwa, const char* przyczyna, bool wOknie) {
    a.port.wiersz(przyczyna ? "BLAD " + nazwa + " " + przyczyna : "OK " + nazwa);
    if (!wOknie) return;
    const std::vector<std::string>& d = plikiZrzutu();
    if (std::find(d.begin(), d.end(), nazwa) == d.end()) return;
    wczytajOdrzucone();
    // lista w NVS od razu (restart w oknie jej nie gubi), zapis tylko przy zmianie
    bool zmiana = przyczyna ? odrzucone.insert(nazwa).second : odrzucone.erase(nazwa) > 0;
    if (zmiana) zapiszOdrzucone();
  }
  // Komenda `W {nazwa} {rozmiar} {sha256}` z treścią (R8). Treść czytana zawsze, gdy rozmiar jest czytelny i w limicie
  // (port zostaje zsynchronizowany także po odrzuceniu). Zwraca true, gdy plik zapisany do `/przywracanie/`.
  // Rozmiar nieczytelny albo ponad limit: treść nie jest czytana (U12: jej bajty idą do pętli jak komendy; narzędzie
  // przerywa po `BLAD … rozmiar`).
  bool komendaW(const std::string& naglowek, bool wOknie) {
    KomendaW kw;
    if (!parsujKomendeW(naglowek, kw)) return false;
    // obrazki dnia (`obrazki/…`), przed logiką przywracania (nie są próbą przywracania); zła nazwa: `BLAD … nazwa`
    if (kw.nazwa.compare(0, strlen(PREFIKS_OBRAZKOW_W), PREFIKS_OBRAZKOW_W) == 0) {
      obrazekW(kw);
      return false;  // nie liczy się do plików przywracania (okno: przywrócenie tylko z plików zrzutu)
    }
    if (kw.nazwa == NAZWA_KONFIGURACJI_W) {  // /konfiguracja.txt, ta sama droga co obrazki
      konfiguracjaW(kw);
      return false;
    }
    // próba przywracania pamiętana w NVS przed treścią (restart w trakcie też się liczy); poza oknem
    // komenda `W` przy znaczniku „bez przywracania” zdejmuje go i liczy się jako próba, więc okno wraca przy następnym
    // starcie zasilania aż do udanego przywrócenia
    if (wOknie && !a.nvs.flaga("cpap", NVS_KLUCZ_PRZYWR_PROBA, false))
      a.nvs.ustawFlage("cpap", NVS_KLUCZ_PRZYWR_PROBA, true);
    if (!wOknie && bezPrzywracania()) {  // to też próba przywracania: od teraz okno wraca do skutku
      a.nvs.usun("cpap", NVS_KLUCZ_PRZYWR);
      a.nvs.ustawFlage("cpap", NVS_KLUCZ_PRZYWR_PROBA, true);
      log("przywracanie: komenda W poza oknem, znacznik „%s” usunięty; okno przywracania przy następnym starcie "
          "zasilania", ZNACZNIK_BEZ_PRZYWRACANIA);
    }
    if (!kw.rozmiarOk) {
      wynikW(kw.nazwa, "rozmiar", wOknie);
      return false;
    }
    // U9: limit całej treści; w oknie przywracania najwyżej do końca okna (T-WYM-87 „WHILE … nie minęło 10 min”)
    uint32_t doMs = ms() + LIMIT_BAJTOW_W_MS + (uint32_t)(kw.rozmiar / 1024) * LIMIT_W_NA_KB_MS;
    if (wOknie && (int32_t)(oknoPrzywrDoMs - doMs) < 0) doMs = oknoPrzywrDoMs;
    std::string t;
    if (!czytajTresc(kw.rozmiar, t, doMs)) {
      log("komenda W %s: %lu z %lu B, przerwa ponad %lu ms albo koniec limitu treści", kw.nazwa.c_str(),
          (unsigned long)t.size(), (unsigned long)kw.rozmiar, (unsigned long)LIMIT_BAJTOW_W_MS);
      // po przerwie (nie po końcu limitu treści ani okna) reszta treści do ciszy, nie jako komendy
      if ((int32_t)(ms() - doMs) < 0)
        doCiszy(wOknie ? oknoPrzywrDoMs : ms() + LIMIT_BAJTOW_W_MS + (uint32_t)(kw.rozmiar / 1024) * LIMIT_W_NA_KB_MS);
      wynikW(kw.nazwa, "rozmiar", wOknie);
      return false;
    }
    if (!wOknie) {  // T-WYM-87: poza oknem bez zapisu
      wynikW(kw.nazwa, "poza oknem", false);
      return false;
    }
    std::string p = sprawdzPlikW(kw, t);
    if (!p.empty()) {
      wynikW(kw.nazwa, p.c_str(), true);
      return false;
    }
    if (!mag.zapiszSurowy(std::string(KATALOG_PRZYWRACANIA) + "/" + kw.nazwa, t)) {
      wynikW(kw.nazwa, "zapis", true);
      return false;
    }
    wynikW(kw.nazwa, nullptr, true);
    return true;
  }
  // plik obrazków dnia z komendy `W` (src/magazyn.h nazwaObrazkaW) do `/obrazki/` zapisem atomowym, w oknie
  // przywracania i w czuwaniu; dane nocy, lista odrzuconych i znaczniki przywracania bez zmian. Rozmiar ponad
  // MAKS_OBRAZKA_W: `BLAD … rozmiar` bez czytania treści (jak rozmiar nieczytelny, U12).
  // rozmiar ponad MAKS_OBRAZKA_W (do MAKS_PLIKU_W) → treść przeczytana i wyrzucona bez bufora, rozmiar
  // nieczytelny albo przerwa w treści → bajty wyrzucane do ciszy (doCiszy()); treść nigdy nie idzie do pętli jako komendy.
  bool obrazekW(const KomendaW& kw) { return plikWBezPrzywracania(kw, MAKS_OBRAZKA_W, false); }
  // `/konfiguracja.txt` z komendy `W konfiguracja.txt` (src/magazyn.h NAZWA_KONFIGURACJI_W): jak obrazki zapis
  // atomowy wprost do pliku, w oknie i w czuwaniu, limit MAKS_KONFIGURACJI_W, skrót SHA-256, zła nazwa `BLAD … nazwa`.
  bool konfiguracjaW(const KomendaW& kw) { return plikWBezPrzywracania(kw, MAKS_KONFIGURACJI_W, true); }
  // Wspólna ścieżka obrazków i konfiguracji (nie są plikami zrzutu: bez listy odrzuconych, NVS i `/przywracanie/`).
  bool plikWBezPrzywracania(const KomendaW& kw, size_t maks, bool konfiguracja) {
    uint32_t doMs = ms() + LIMIT_BAJTOW_W_MS + (uint32_t)(kw.rozmiar / 1024) * LIMIT_W_NA_KB_MS;
    if (!kw.rozmiarOk) {
      doCiszy();
      wynikW(kw.nazwa, "rozmiar", false);
      return false;
    }
    if (kw.rozmiar > maks) {
      if (!wyrzucTresc(kw.rozmiar, doMs)) doCiszy();
      wynikW(kw.nazwa, "rozmiar", false);
      return false;
    }
    std::string t;
    if (!czytajTresc(kw.rozmiar, t, doMs)) {
      doCiszy();
      wynikW(kw.nazwa, "rozmiar", false);
      return false;
    }
    std::string p = konfiguracja ? sprawdzKonfiguracjeW(kw, t) : sprawdzObrazekW(kw, t);
    if (!p.empty()) {
      wynikW(kw.nazwa, p.c_str(), false);
      return false;
    }
    std::string sciezka = "/" + kw.nazwa;
    if (!mag.zapiszSurowy(sciezka, t)) {
      wynikW(kw.nazwa, "zapis", false);
      return false;
    }
    log("%s: zapisano %s (%lu B, komenda W)", konfiguracja ? "konfiguracja" : "obrazki", sciezka.c_str(),
        (unsigned long)t.size());
    wynikW(kw.nazwa, nullptr, false);
    return true;
  }
  // Wiersz stanu okna przywracania: log przy otwarciu i odpowiedź na `?` w oknie (U3: narzędzie pyta, zamiast czekać
  // na wiersz wypisany, zanim otworzyło port).
  // Na U-05 (ramka) `Okno przywracania…`, w logu i odpowiedzi na `?` małą literą: tools/prototyp/wgraj_pliki.py
  // szuka `okno przywracania( do hh:mm|: m min)`.
  std::string wierszOkna() {
    std::string o = tekstOknaPrzywracania();
    o[0] = 'o';
    return o + " (T-WYM-87): port serwisowy, komenda W, K kończy; bez Wi-Fi";
  }

  // T-WYM-87: okno przywracania (bez Wi-Fi i pobierania), komendy `W`, `K` i `?` z portu serwisowego, potem
  // przywrócenie (T-WYM-62), gdy przyszedł co najmniej jeden plik albo `K` zastało komplet. Ekran testu z linią okna na
  // czas okna. Koniec: `K`, dowolny przycisk, 10 min (także w trakcie treści komendy `W`, U9).
  // `K` przy komplecie plików (kompletPlikow) zapisuje
  // `przywr_k` przed przywróceniem, a dopiero wtedy przywroc() może postawić znacznik; okno zakończone (czas,
  // przycisk, `K`) bez żadnej komendy `W` w tym ani we wcześniejszym oknie (`przywr_proba`) stawia znacznik „bez
  // przywracania”, więc nowy egzemplarz bez zrzutu ma okno tylko raz.
  void oknoPrzywracania() {
    uint32_t t0 = ms();
    oknoPrzywr = true;
    oknoPrzywrDoMs = t0 + OKNO_PRZYWRACANIA_MS;
    wczytajOdrzucone();
    if (a.nvs.flaga("cpap", NVS_KLUCZ_PRZYWR_K, false)) a.nvs.usun("cpap", NVS_KLUCZ_PRZYWR_K);  // `K` dotyczy okna
    log("%s", wierszOkna().c_str());
    ustawEkran(Ekran::TEST);
    pokaz();
    int przyjete = 0;
    bool komplet = false;
    std::string w;
    for (;;) {
      odnow();
      if (ms() - t0 >= OKNO_PRZYWRACANIA_MS) {
        log("okno przywracania: koniec czasu");
        break;
      }
      if (wcisniety() != Przycisk::BRAK) {  // przycisk kończy okno; naciśnięcie zużyte na zamknięcie okna
        log("okno przywracania: zakończone przyciskiem");
        czekajNaPuszczenie();
        break;
      }
      int c = a.port.znak();
      if (c < 0) {
        czekaj(OBIEG_CZUWANIA_MS);
        continue;
      }
      if (c == '\n' || c == '\r') {
        if (!w.empty() && komendaW(w, true)) przyjete++;
        w.clear();
        continue;
      }
      if (w.empty() && c == 'K') {
        komplet = kompletPlikow();
        if (komplet) a.nvs.ustawFlage("cpap", NVS_KLUCZ_PRZYWR_K, true);  // przed przywróceniem (zanik zasilania)
        log("okno przywracania: zakończone komendą K (%s)",
            komplet ? "komplet plików" : odrzucone.empty() ? "bez pliku nocy" : "pliki odrzucone, bez kompletu");
        break;
      }
      if (w.empty() && c == '?') {  // U3: zapytanie o stan okna
        a.port.wiersz(wierszOkna());
        continue;
      }
      if (w.size() < MAKS_WIERSZA_KOMENDY) w += (char)c;
    }
    oknoPrzywr = false;
    ustawEkran(Ekran::DZIEN);
    if (!a.nvs.flaga("cpap", NVS_KLUCZ_PRZYWR_PROBA, false)) {  // nikt nie próbował przywracać
      std::string z = std::string(ZNACZNIK_BEZ_PRZYWRACANIA) + " " + (dzisiaj().empty() ? std::string("?") : dzisiaj());
      a.nvs.ustawTekst("cpap", NVS_KLUCZ_PRZYWR, z);
      log("okno przywracania: bez komendy W, znacznik „%s” w NVS (okno się już nie otworzy; komenda W poza oknem "
          "przywraca je przy następnym starcie zasilania), praca bez historii", z.c_str());
      return;
    }
    if (!przyjete && !komplet) {
      log("okno przywracania: bez plików, praca bez historii");
      return;
    }
    przywroc();
    if (przywrocenie.ok) wczytajDane();
  }

  void pokaz() {
    zaOknemKlatki = NAN;  // ustawia je tylko slot dzienny
    zapiszOdlozone();  // R-DANE-04: rekord nocy w plikach przed odświeżeniem (gdy Wi-Fi zostało dla okna serwisowego)
    std::string dzis = dzisiaj();
    switch (ekran()) {
      case Ekran::K1: {
        DaneK1 k1 = budujK1(hist, dzis, norma(dzis), czynniki, &stany(dzis), r.ab.tryb);
        k1.ocena = ocenaNaglowkaK1(stanyOcenyB5(dzis));  // R-KOK-21: stan po ocenie silnika
        // R-POR-24, R-PRZ-04: K1 otwarte ze strony Portretu: ◀ wraca do Portretu (przejsciePortretu()), więc tak podpisane
        if (r.k1ZPortretu && r.portretStrona) {
          k1.stopkaWstecz = sk::tekst("S11.stopka_wstecz");
          log("stopka K1: %s (R-POR-24)", k1.stopkaWstecz.c_str());
        }
        wczytajK1OdkrycieB15();  // po uśpieniu z NVS
        if (k1OdkrycieB15.dzien == dzis) {  // R-CZY-20: K1 z ekranu dnia O-12 z ukrytym czynnikiem
          k1.odkrycieTytul = k1OdkrycieB15.tytul;
          k1.odkryciePowod = k1OdkrycieB15.powod;
        }
        rysujK1(plotno(), k1);
        break;
      }
      case Ekran::K2: {
        DaneK2 k2 = budujK2(hist, dzis, norma(dzis), czynniki, &stany(dzis), r.ab.tryb);
        k2.sprawy = sprawyWCiaguB5(dzis);  // jak KontekstNaw::sprawyDostepne (R-KOK-19)
        k2.liczby = profilB18();          // R-KOK-22
        rysujK2(plotno(), k2);
        break;
      }
      case Ekran::PYTANIE: rysujPytanieTeraz(); break;
      case Ekran::TEST: rysujTestTeraz(); break;
      case Ekran::LICZBY: rysujLiczbyB15(dzis); break;  // „Co znaczą te liczby” (R-KOK-22)
      case Ekran::OMOWIONE:
      case Ekran::SPRAWY:
      case Ekran::PRZENIESIONE:
      case Ekran::ZAPISANO:
        if (!pokazB5(dzis)) rysujDzien();
        break;
      case Ekran::RAPORT: rysujRaportB13(dzis); break;
      case Ekran::SPRZET_ZAPISANO: rysujWymianeZapisanaB16(); break;  // R-MAS-01, R-MAS-18
      case Ekran::USTAWIENIA: rysujUstawienia(plotno(), daneU01()); break;  // U-01 (R-KONF-55)
      case Ekran::FORMULARZ: rysujFormularzP07(); break;                                    // U-03
      case Ekran::SIEC_PANELU: rysujSiecPaneluP07(); break;                                 // U-02
      case Ekran::AKTUALIZACJA: rysujU07Teraz(); break;                                     // U-07
      case Ekran::PORTRET: rysujPortretStrony(dzis); break;                                // R-POR-24
      default:
        if (ekran() == Ekran::DZIEN && k.pierwszaKonfiguracja) rysujSiecPaneluP07();  // instrukcja zamiast E-01
        else rysujDzien();
        break;
    }
    odnow();
    // R-EKR-60: chwila ostatniego narysowania ekranu dnia do planu (0 = bez przerysowań: S5, S10, nocny, bez czasu);
    // także przy klatce bez zmian, bo kolejne 2 h liczą się od tej oceny
    if (ekran() == Ekran::DZIEN) r.dzienRysowanyEp = dzienOdswiezalny && zegar() ? teraz() : 0;
    // Klatka identyczna z wyświetloną nie odświeża e-ink. Zawsze odświeża: pierwsze
    // rysowanie po starcie innym niż wybudzenie z uśpienia, po zmianie ekranu, ekran testu, wersja do sprawdzenia
    // (samosprawdzenie T-WYM-41), bez zegara i gdy od ostatniego odświeżenia minęło KLATKA_NAJDLUZEJ_S.
    uint32_t skrot = crc32(a.wyswietlacz.bufor(), BAJTOW_KLATKI);
    bool zawsze = ekran() == Ekran::TEST || samosprawdzenieWymagane(a.uruchomienie.stanWersji()) || !zegar();
    if (!zawsze && r.klatkaOd != 0 && r.klatkaSkrot == skrot && teraz() >= r.klatkaOd &&
        teraz() - r.klatkaOd < KLATKA_NAJDLUZEJ_S) {
      log("ekran %d bez zmian: klatka jak wyświetlona, bez odświeżenia e-ink", (int)r.ekran);
      return;
    }
    if (stopkaZPamieci) {  // ekran się zmienił, więc odświeżenie e-ink ma już świeży odczyt czujnika
      stopkaZPamieci = false;
      pokaz();
      return;
    }
    if (!ekranWlaczony) {
      a.wyswietlacz.wlacz();
      ekranWlaczony = true;
    }
    uint32_t t0 = ms();
    a.wyswietlacz.odswiez();
    odnow();
    r.klatkaSkrot = skrot;
    r.klatkaOd = zegar() ? teraz() : 0;
    r.stopkaTemp = tempStopki();  // stopka tej klatki
    r.stopkaWilg = wilgStopki();
    r.stopkaZaOknem = zaOknemKlatki;
    log("ekran %d narysowany w %lu ms", (int)r.ekran, (unsigned long)(ms() - t0));
  }

  // ---------- AirBridge od v2.2.0 ----------
  void abPrzerywanie(bool wl) {
    abPrzerywalne = wl;
    abDetektor = PrzerwaniePrzyciskiem();
    abPrzerwano = false;
  }
  bool abPrzerwij() {
    if (abPrzerywalne && !abPrzerwano && abDetektor.krok(wcisniety() != Przycisk::BRAK)) {
      abPrzerwano = true;
      // naciśnięcie, które przerwało pobieranie, nie wraca z kolejki; przy pobraniu rano po przycisku (R-PRZ-22)
      // wraca, bo to zwykła nawigacja, a pobranie było tylko dodatkiem
      if (!abZostawKolejke) a.przyciski.wyczyscKolejke();
      log("AirBridge: pobieranie przerwane przyciskiem");
    }
    return abPrzerwano;
  }
  long abZostalo() { return (long)(int32_t)(abKoniecMs - ms()); }

  std::string cnonce() {
    char b[17];
    snprintf(b, sizeof b, "%08lx%08lx", (unsigned long)a.siec.losowa(), (unsigned long)a.siec.losowa());
    return b;
  }

  // Jedna próba GET z logowaniem Digest.
  // Jedno żądanie GET do AirBridge (z drugim podejściem tylko po 401 Digest). `limitMs`: limit odpowiedzi (domyślnie
  // AB_LIMIT_ODPOWIEDZI_MS; ocena slotu dziennego LIMIT_STATUSU_SLOTU_MS). Zwraca kod HTTP albo kod błędu ≤ 0.
  // `maksBajtow`: limit ciała; 0 = z trasy (limitOdpowiedziAB()), przy odpytywaniu joba
  // limit trasy pierwotnej (abRaport()).
  int abGetRaz(const std::string& sciezka, std::string& tresc, uint32_t limitMs = AB_LIMIT_ODPOWIEDZI_MS,
               size_t maksBajtow = 0) {
    tresc.clear();
    if (!maksBajtow) maksBajtow = limitOdpowiedziAB(sciezka);
    for (int proba = 0; proba < 2; proba++) {
      ZadanieHttp z;
      z.url = "http://" + k.abAdres + sciezka;
      z.limitMs = limitMs;
      z.maksBajtow = maksBajtow;  // bez limitu duża odpowiedź wyczerpuje stertę
      if (abWyzwanie.ok)
        z.naglowki.emplace_back("Authorization", naglowekDigest(abWyzwanie, k.abKonto, k.abHaslo, "GET", sciezka,
                                                                ++abNc, cnonce(), a.siec.md5()));
      odnow();
      uint32_t t0 = ms();
      OdpowiedzHttp o = a.siec.get(z);
      odnow();
      int kod = o.kod;
      if (kod == 401) {
        httpMs = ms() - t0;
        WyzwanieDigest w = parsujWyzwanie(o.wwwAuthenticate);
        bool bylo = abWyzwanie.ok;
        abWyzwanie = w;
        abNc = 0;
        if (!w.ok || (proba == 1 && bylo)) {
          log("AirBridge %s: 401, logowanie Digest odrzucone (sprawdź konto i hasło AirBridge)", sciezka.c_str());
          return 401;
        }
        continue;
      }
      if (kod > 0 && o.zaDuzo) {  // przed cialoKompletne(): przerwany strumień to nie urwane połączenie
        httpMs = ms() - t0;
        log("AirBridge %s: HTTP %d, odpowiedź ponad %u B odrzucona (Content-Length %d)", sciezka.c_str(), kod,
            (unsigned)z.maksBajtow, o.contentLength);
        return AB_KOD_ZA_DUZO;  // nieudana próba bez ponowień (ponowicZadanie())
      }
      if (kod > 0) {
        tresc = std::move(o.cialo);
        if (!cialoKompletne(o.contentLength, o.wynikZapisu, tresc.size())) {
          log("AirBridge %s: HTTP %d, ciało urwane (%u z %d B, wynik %d)", sciezka.c_str(), kod, (unsigned)tresc.size(),
              o.contentLength, o.wynikZapisu);
          kod = AB_KOD_URWANE;
          tresc.clear();
        }
      }
      httpMs = ms() - t0;
      log("AirBridge %s: HTTP %d w %lu ms, %u B", sciezka.c_str(), kod, (unsigned long)httpMs, (unsigned)tresc.size());
      return kod;
    }
    return 401;
  }

  // R-AW-01: do 3 prób w odstępie 10 s w granicach budżetu wybudzenia.
  // `maksBajtow`: jak w abGetRaz() (0 = limit z trasy `sciezka`).
  int abGet(const std::string& sciezka, std::string& tresc, size_t maksBajtow = 0) {
    int kod = 0;
    for (int proba = 1;; proba++) {
      kod = abGetRaz(sciezka, tresc, AB_LIMIT_ODPOWIEDZI_MS, maksBajtow);
      if (!ponowicZadanie(kod, proba, abZostalo())) break;
      log("AirBridge %s: brak odpowiedzi (%d), próba %d z %d za 10 s", sciezka.c_str(), kod, proba + 1, AB_PROB_ZADANIA);
      for (uint32_t c = ms(); ms() - c < AB_ODSTEP_PROB_MS;) {
        odnow();
        if (abPrzerwij()) return kod;
        czekaj(OBIEG_CZUWANIA_MS);
      }
    }
    if (brakOdpowiedzi(kod)) log("AirBridge %s: nieudana próba pobrania (kod %d)", sciezka.c_str(), kod);
    return kod;
  }

  // Raport z odpytywaniem joba co 5 s (R-DANE-11).
  Odpowiedz abRaport(const std::string& sciezka, std::string& tresc, uint32_t limitMs, int& kod, bool* przerwane) {
    if (przerwane) *przerwane = false;
    uint32_t t0 = ms();
    const size_t maksBajtow = limitOdpowiedziAB(sciezka);  // job dziedziczy limit trasy (lista 512 KiB)
    kod = abGet(sciezka, tresc, maksBajtow);
    if (abPrzerwano) {
      if (przerwane) *przerwane = true;
      return Odpowiedz::BLAD;
    }
    OdpowiedzAB o = klasyfikujOdpowiedz(kod, tresc);
    std::string job = o.job;
    while (o.r == Odpowiedz::W_BUDOWIE) {
      if (koniecOdpytywania(ms() - t0, limitMs, abZostalo())) {
        log("AirBridge %s: raport w budowie po %lu s (job %s)", sciezka.c_str(), (unsigned long)((ms() - t0) / 1000),
            job.c_str());
        return Odpowiedz::W_BUDOWIE;
      }
      for (uint32_t c = ms(); ms() - c < AB_ODPYTYWANIE_MS;) {
        odnow();
        if (abPrzerwij()) {
          log("AirBridge %s: przerwane przyciskiem (job %s)", sciezka.c_str(), job.c_str());
          if (przerwane) *przerwane = true;
          return Odpowiedz::W_BUDOWIE;
        }
        czekaj(OBIEG_CZUWANIA_MS);
      }
      kod = abGet(job.empty() ? sciezka : sciezkaJoba(job), tresc, maksBajtow);
      if (abPrzerwano) {
        if (przerwane) *przerwane = true;
        return Odpowiedz::W_BUDOWIE;
      }
      o = klasyfikujOdpowiedz(kod, tresc);
      if (!o.job.empty()) job = o.job;
    }
    if (o.r == Odpowiedz::BLAD && kod == 200) log("AirBridge %s: 200 z ciałem nieczytelnym", sciezka.c_str());
    return o.r;
  }

  void zapiszStanAB() {
    a.nvs.ustawBajty("noc", "ab", &r.ab, sizeof r.ab);
    a.nvs.usun("noc", "noc");  // stary zapis (inny układ)
  }

  // R-DANE-24: ab_rano, ab_11 w NVS „cpap”
  std::string nvsTekst(const char* klucz) { return a.nvs.tekst("cpap", klucz, ""); }
  void nvsTekst(const char* klucz, const std::string& v) { a.nvs.ustawTekst("cpap", klucz, v); }

  void wczytajZalegle(int& gotowe, uint32_t& czekajDo) {
    gotowe = a.nvs.liczba("cpap", "ab_zal", -1);
    czekajDo = a.nvs.liczbaU("cpap", "ab_czd", 0);
  }
  void zapiszZalegle(const Zalegle& z) {
    int g;
    uint32_t c;
    wczytajZalegle(g, c);
    if (g != z.gotowe || c != z.czekajDo) {  // zapis tylko przy zmianie (pamięć flash)
      a.nvs.ustawLiczbe("cpap", "ab_zal", z.gotowe);
      a.nvs.ustawLiczbeU("cpap", "ab_czd", z.czekajDo);
    }
    r.ab.zalegle = z.gotowe;
    r.ab.czekajace = z.czekajace;
  }

  // Stan pobierania historii: z plików raz na wybudzenie, potem w pamięci (zmiany czekają na zapiszOdlozone()).
  StanHistAB& wczytajStanHist() {
    if (!shWczytany) {
      sh.pom = parsujPominiete(bezZapisu.count("pominiete") ? czytajWiersze("/ab_pominiete.txt") : wczytaj("pominiete"));
      sh.proby = parsujProby(bezZapisu.count("proby") ? czytajWiersze("/ab_proby.txt") : wczytaj("proby"));
      shWczytany = true;
    }
    return sh;
  }
  // W sesji Wi-Fi (odlozZapis) tylko zostawia flagi zmian; zapis po wyłączeniu Wi-Fi (7.1 krok 10, T-NF-06).
  void zapiszStanHist(StanHistAB& s) {
    if (odlozZapis) return;
    bool histOk = true;
    if (s.histZm) {  // R-DANE-04: zapis przed odświeżeniem ekranu
      sortuj();
      histOk = zapiszHistorie();
    }
    if (!strEtagCzeka.empty()) {  // ADR-0035 pkt 1: etag STR dopiero po udanym zapisie historii
      if (histOk) a.nvs.ustawTekst("cpap", NVS_KLUCZ_STR_ETAG, strEtagCzeka);
      else log("STR: historia niezapisana, etag bez zmian (STR pobrany ponownie w następnym wybudzeniu z listą)");
      strEtagCzeka.clear();
    }
    if (s.pomZm) zapiszFormat("pominiete", mag.sciezkaRoku("pominiete", ""), wierszePominietych(s.pom));
    if (s.probyZm) zapiszFormat("proby", mag.sciezkaRoku("proby", ""), wierszeProb(s.proby));
    zapiszPorownanie();
    s.histZm = s.pomZm = s.probyZm = false;
  }
  // /dane/porownanie.csv (tryb porównawczy): zapis odłożony jak historia, tylko na partycji zapisywalnej
  void zapiszPorownanie() {
    if (odlozZapis || !porZm) return;
    if (zapisB5()) zapiszFormat("porownanie", mag.sciezkaRoku("porownanie", ""), wierszeZ(porWpisy, porownanie::wierszWpisu));
    porZm = false;
  }
  void zapiszOdlozone() {
    odlozZapis = false;
    if (shWczytany) zapiszStanHist(sh);
    zapiszPorownanie();
    zapiszPortretCzekajacy();  // Portret nocy: po historii
  }
  // Wi-Fi wyłączone zaraz po ostatnim żądaniu (7.1 krok 9, T-WYM-20, T-NF-06), potem zapis odłożony (krok 10).
  void zamknijSiec() {
    if (wifiOdMs) {
      wifiLacznieMs += ms() - wifiOdMs;
      wifiOdMs = 0;
    }
    if (wifiProbowano) {  // wybudzenie pomiarowe i przycisk bez sieci: radio nie było włączane, bez wywołań Wi-Fi
      a.siec.wylacz();
      odnow();
    }
    zapiszOdlozone();
  }
  void aktualizujZalegle(const StanHistAB& s, const std::string& dzis) {
    if (abListaOk) zapiszZalegle(policzZalegle(abLista, hist, s.pom, dzis, s.proby, teraz()));
  }
  // STR.edf do ustawień nocy z plików sesji (ADR-0036): najwyżej jedno pobranie na wybudzenie, wspólne ze
  // strZAirBridge(). nullptr: pobranie albo odczyt nieudany (noc z raportu JSON).
  const Str* strDoNocy() {
    if (strWybOk) return &strWyb;
    if (strWybProbowano) return nullptr;
    strWybProbowano = true;
    std::string etag = strEtagListy;  // odczytany przed pobraniem: nie nowszy niż treść pliku
    std::string t;
    int kod = abGetRaz(sciezkaPobraniaStr(), t);
    if (kod != 200) {
      log("noc z plików: STR.edf niepobrany (kod %d)", kod);
      return nullptr;
    }
    Str st;
    edf::Blad b = czytajStr(std::move(t), st);
    if (b != edf::Blad::BRAK) {
      log("noc z plików: STR.edf nieczytelny (%s)", edf::opisBledu(b));
      return nullptr;
    }
    strWyb = std::move(st);
    strWybOk = true;
    strWybEtag = etag;
    return &strWyb;
  }
  // ---------- Portret nocy: plik przebiegu ostatniej nocy (T-DANE-38, src/portret.h) ----------
  // Data poranka w pliku `/dane/portret.csv` ("" = brak pliku albo plik uszkodzony); czytane raz na wybudzenie.
  std::string dataPortretu() {
    if (!portretWczytany) {
      portretWczytany = true;
      std::string blad;
      przebiegNocy = portret::Przebieg();
      // przez magazyn wprost (nie wczytaj(), który pomija plik uszkodzony bez słowa): R-POR-21
      OdczytPliku o = fsOk() ? mag.czytaj("portret", "/dane/portret.csv") : OdczytPliku();
      if (o.stan == StanPliku::USZKODZONY) {
        log("portret: plik uszkodzony (nagłówek, nazwy pól albo #koniec), rano ekran z 7.2");
      } else if ((o.stan == StanPliku::DOBRY || o.stan == StanPliku::NOWSZY) && !o.wiersze.empty() &&
                 !portret::parsuj(o.wiersze, przebiegNocy, blad)) {
        log("portret: plik uszkodzony (%s), rano ekran z 7.2", blad.c_str());
        przebiegNocy = portret::Przebieg();
      }
    }
    return przebiegNocy.data;
  }
  // Doba `day` jest ostatnią nocą w historii (nie starsza od żadnej nocy w historii ani od przebiegu w pliku).
  bool portretDoby(long day) {
    std::string d = dataPoranka(day);
    for (auto& n : hist)
      if (n.data > d) return false;
    return dataPortretu() <= d && (!portretCzeka || portretDoZapisu.data <= d);
  }
  // Przebieg nocy do zapisu: w sesji Wi-Fi (odlozZapis) czeka do zapiszOdlozone() jak historia (T-NF-06); przy
  // starym układzie partycji (staryUklad) bez zapisu jak inne pliki B5 (zapisB5()).
  void zapiszPortret(const portret::Przebieg& p, size_t pamiec) {
    portretDoZapisu = p;
    portretCzeka = true;
    portretPamiec = pamiec;
    zapiszPortretCzekajacy();
  }
  void zapiszPortretCzekajacy() {
    if (!portretCzeka || odlozZapis) return;
    portretCzeka = false;
    const portret::Przebieg& p = portretDoZapisu;
    std::vector<std::string> w = portret::wiersze(p);
    size_t b = 0;
    for (auto& x : w) b += x.size() + 1;
    bool ok = zapisB5() && zapiszFormat("portret", "/dane/portret.csv", w);
    if (ok) {
      przebiegNocy = p;
      portretWczytany = true;
    }
    log("portret: przebieg nocy %s %s (%u odcinków, %u zdarzeń, braki „%s”, ok. %u B pliku, pamięć zbieracza %u B)",
        p.data.c_str(), ok ? "zapisany" : "NIE zapisany", (unsigned)p.odcinki.size(), (unsigned)p.zdarzenia.size(),
        p.braki.c_str(), (unsigned)b, (unsigned)portretPamiec);
  }
  // ---------- Portret nocy na ekranie (S11, spec 5.4.16, ADR-0040; src/portret_ekran.h) ----------
  static const uint32_t PORTRET_DRUGI_S = 30 * 60;  // drugi ekran poranka po 30 min (R-POR-04) [PRÓG]
  // Doba poranka z Portretem i chwila pierwszego narysowania (RTC, po zaniku zasilania z kopii w NVS).
  void wczytajPoranekPortretu() {
    if (r.portretDoba != 0) return;
    r.portretDoba = (int32_t)a.nvs.liczbaU("cpap", "por_db", 0);
    r.portretOdEp = a.nvs.liczbaU("cpap", "por_od", 0);
  }
  void zapiszPoranekPortretu() {
    a.nvs.ustawLiczbeU("cpap", "por_db", (uint32_t)r.portretDoba);
    a.nvs.ustawLiczbeU("cpap", "por_od", r.portretOdEp);
  }
  // Chwila pierwszego narysowania Portretu dziś; 0 = dziś jeszcze nie było.
  uint32_t portretOdDzis(const std::string& dzis) {
    wczytajPoranekPortretu();
    return dataOk(dzis) && r.portretDoba == (int32_t)(nrDnia(dzis) + 1) ? r.portretOdEp : 0;
  }
  // Przebieg ostatniej nocy jest z nocy z poranka `dzis` i jest w historii (R-POR-03), noc z terapią (R-POR-16).
  bool portretRanoDostepny(const std::string& dzis) {
    if (dzis.empty() || !zegar()) return false;
    const Noc* noc = nocZDnia(hist, dzis);
    const Noc* ost = ostatniaNoc(hist, "9999-99-99");
    return noc && ost == noc && dataPortretu() == dzis && portret::jestPortret(przebiegNocy);
  }
  // Poranek z Portretem trwa od pierwszego narysowania do startu slotu dziennego (R-POR-04, R-KON-41).
  bool portretWToku(const std::string& dzis) {
    uint32_t od = portretOdDzis(dzis);
    return od != 0 && sekundaDzis(r.slotDziennyEp) < 0 && teraz() >= od && teraz() < od + PORTRET_DRUGI_S;
  }
  // Minuta doby drugiego ekranu poranka do planu (-1 = brak).
  int minutaDrugiegoEkranu(const std::string& dzis) {
    uint32_t od = portretOdDzis(dzis);
    if (od == 0 || sekundaDzis(r.slotDziennyEp) >= 0) return -1;
    int s = sekundaDzis(od);  // chwila przeszła (sekundaDzis() nie liczy chwil po teraz)
    if (s < 0) return -1;
    int m = (s + (int)PORTRET_DRUGI_S + 59) / 60;
    return m < 24 * 60 ? m : -1;
  }
  // Pierwszy podpis: zdanie o normie z reguł zdania S2a (R-POR-12, tabela „Podpisy” 5.4.16).
  std::string pierwszyPodpisPortretu(const TrescEkranu& ts, const DaneDnia& d) {
    // rozruch globalny jak S6 (src/humor.cpp wybierzEkranDnia(): < 14 nocy ważnych albo bez normy czasu z maską)
    if (!d.hist || liczbaNocyWaznych(*d.hist, d.dzis) < 14 || !d.norma.m01.jest())
      return sk::tekst("S11.rozruch", {{"n", std::to_string(d.norma.n < 14 ? d.norma.n : 13)}});
    std::vector<const Kolumna*> poza;
    for (auto& kol : ts.kolumny)
      if (kol.poza) poza.push_back(&kol);
    std::string z;
    if (poza.empty()) {
      z = sk::tekst("S11.tydzien_zwykle");
    } else if (poza.size() == 1) {
      std::string m = poza[0]->et;
      size_t j = m.find(" l/min");
      if (j != std::string::npos) m = m.substr(0, j);
      z = sk::tekst("S11.tydzien_jedna", {{"metryka", m}, {"kierunek", poza[0]->s == "↑" ? "wyżej" : "niżej"}});
    } else {
      z = sk::tekst("S11.tydzien_wiele", {{"n", std::to_string(poza.size())}});
    }
    return ts.wNormie ? sk::tekst("S11.w_normie") + " " + z : z;
  }
  // M-18 nocy z raportu JSON (`A`) do 09.11.2026 liczył start pierwszej sesji doby (także drzemki), a nie start
  // najdłuższej sesji od 18:00 (R-DANE-27); poprawione 09.11.2026, więc te noce pomija mediana
  // zwykłej pory startu. Noce z plików sesji, z karty i ze STR mają M-18 według R-DANE-27.
  static bool m18Wiarygodne(const Noc& n) { return !(n.zrodlo == 'A' && n.data <= M18_POPRAWNE_DO); }
  static constexpr const char* M18_POPRAWNE_DO = "2026-11-09";
  int zwyklaPoraPortretu(const std::string& dzis) {
    std::vector<std::pair<std::string, int>> pd;  // (data, M-18) nocy przed ostatnią
    for (auto& x : hist)
      if (x.data < dzis && x.m18 >= 0 && m18Wiarygodne(x)) pd.push_back({x.data, x.m18});
    std::sort(pd.begin(), pd.end());
    if (pd.size() > 14) pd.erase(pd.begin(), pd.end() - 14);
    std::vector<int> v;
    v.reserve(pd.size());
    for (auto& x : pd) v.push_back(x.second);
    std::sort(v.begin(), v.end());
    if (v.size() >= 3) return v[(v.size() - 1) / 2];  // mediana dolna (jak tools/portret median_low)
    const Noc* noc = nocZDnia(hist, dzis);
    if (noc && noc->m18 >= 0) return noc->m18;
    return przebiegNocy.sesje.empty() ? 11 * 60 : przebiegNocy.sesje.front().od;
  }
  DanePortretu danePortretu(const TrescEkranu& ts, const DaneDnia& d, const std::string& dzis) {
    DanePortretu dp;
    dp.przebieg = &przebiegNocy;
    const Noc* noc = nocZDnia(hist, dzis);
    dp.tryb = noc ? trybNocy(*noc) : r.ab.tryb;
    if (dp.tryb.empty()) dp.tryb = r.ab.tryb;
    const Norma& nm = d.stany ? d.stany->norma : d.norma;
    if (nm.psmed.jest() && !std::isnan(nm.psmed.med)) dp.normaPs10 = (int)std::floor(nm.psmed.med * 10 + 0.5);
    dp.zwykla = zwyklaPoraPortretu(dzis);
    dp.pierwszy = pierwszyPodpisPortretu(ts, d);
    dp.czas = ts.czas;
    dp.ahi = ts.ahi;
    dp.niepewna = ts.ahiNiepewne;
    return dp;
  }
  // R-POR-02, R-POR-04: Portret jako ekran dnia rano (zwycięzca z R5e–R9 po 30 min). false = rysuje dalej zwykły
  // wybór (brak przebiegu, problem poranny, slot dzienny, po 30 min drugi ekran).
  bool rysujPortretB40(const EkranDnia& e, DaneDnia& d, const Naglowek& n, const std::string& dzis) {
    if (e.typ == "S5" || e.typ == "S10" || !portretRanoDostepny(dzis)) return false;
    int m = minutaDoby();
    if (m < 5 * 60 || sekundaDzis(r.slotDziennyEp) >= 0) return false;
    if (slot_wieczorny::wOknie(slot_wieczorny::oknoZHistorii(hist, dzis), m)) return false;
    sk::Wejscie w = wejscieB16(dzis, d);  // tu już przed startem slotu dziennego: strona pod ▶ tylko „rano”
    for (auto& x : sk::rano(w)) {  // problem poranny z R4–R5d ma pierwszeństwo; Portret pod ▶ (R-POR-24)
      if (x.doLogu || x.id == "S-K-30") continue;
      if (sk::ranga(x.grupa) <= sk::ranga("R5d")) {
        r.portretStrona = 1;
        return false;
      }
    }
    uint32_t od = portretOdDzis(dzis);
    if (od == 0) {  // pierwsze narysowanie tego ranka
      r.portretDoba = (int32_t)(nrDnia(dzis) + 1);
      r.portretOdEp = teraz();
      r.portretDrugi = 0;
      r.slotOdlozony = 0;
      zapiszPoranekPortretu();
      od = r.portretOdEp;
    }
    if (teraz() >= od + PORTRET_DRUGI_S) {
      if (!r.portretDrugi) log("portret: po 30 min drugi ekran poranka (R-POR-04)");
      drugiEkranTeraz = !r.portretDrugi;
      r.portretDrugi = 1;
      // (R-POR-24): na drugim ekranie zwykłego poranka ▶ otwiera stronę Portretu przed K1,
      // jak w dni problemu, do startu slotu dziennego; stopka ekranu mówi `▶ portret` przed odczytem czujnika (dane dnia,
      // z których stopkę biorą wszystkie ścieżki rysowania niżej). Bez strony, gdy slot dzienny startuje zaraz po tym
      // rysowaniu (start odłożony do drugiego ekranu), bo wtedy ▶ prowadzi już do K1.
      if (!(drugiEkranTeraz && r.slotOdlozony)) {
        r.portretStrona = 1;
        d.przyLozku = prawoZPortretem(d.przyLozku);
        dopasujStopke(d.pogoda, d.przyLozku);  // R-EKR-10: najpierw ustępuje opis pogody i miasto
        log("portret: drugi ekran poranka, Portret pod ▶ (R-POR-24)");
      }
      return false;
    }
    TrescEkranu t = trescDnia("S2A", "", d);
    DanePortretu dp = danePortretu(t, d, dzis);
    t.uklad = Uklad::PORTRET;
    t.portret = &dp;
    uint32_t t0 = ms();
    rysujEkranDanych(plotno(), n, t);
    uint32_t czas = ms() - t0;
    dzienOdswiezalny = t.stopkaStandardowa;
    r.dzienTryb = 0;  // następne rysowanie (drugi ekran) liczy tryb od nowa
    r.dzienLinia = 0;
    if (!wpisDzis("S11", dzis)) dopiszDziennik({dzis, "S11", "", 'D', "", ""});  // R-POR-01; poza limitami (R-POR-20)
    WpisCzynnikow* zap = wpisZDnia(czynniki, dzis);
    if (oznaczycZaproszenie(t.linia == TEKST_ZAPROSZENIA, zap)) {  // R-PIE-08 jak na ekranie dnia
      zap->pokazano = true;
      zapiszCzynniki();
    }
    log("ekran dnia S11 (Portret nocy, %s): %s, rysowanie %lu ms (T-WYM-97); nagłówek „%s”", dp.tryb.c_str(),
        portret::podpisy(dp, !t.linia.empty()).front().c_str(), (unsigned long)czas, n.prawo.c_str());
    return true;
  }
  // R-POR-24: Portret jako strona pod ▶ z ekranu problemu (bez numeru strony); stopka z akcjami ◀ ▶ ↻.
  void rysujPortretStrony(const std::string& dzis) {
    if (!portretRanoDostepny(dzis)) {
      ustawEkran(Ekran::DZIEN);
      rysujDzien();
      return;
    }
    DaneDnia d = daneDnia(dzis);
    Naglowek n{naglowekDniaLewo(d), naglowekDniaPrawo(d)};
    TrescEkranu t = trescDnia("S2A", "", d);
    DanePortretu dp = danePortretu(t, d, dzis);
    t.uklad = Uklad::PORTRET;
    t.portret = &dp;
    t.linia.clear();
    t.stopkaLewo.clear();
    t.stopkaPrawo.clear();
    rysujEkranDanych(plotno(), n, t);
    plotno().prostokat(0, 434, 800, 46, false);
    stopkaEkranu(plotno(), {"◀ ekran dnia", "▶ ostatnia noc", "↻ ekran dnia"}, "");
    log("Portret nocy jako strona pod ▶ (R-POR-24): %s; nagłówek „%s”", portret::podpisy(dp, false).front().c_str(),
        n.prawo.c_str());
  }
  // R-POR-24: przejścia strony Portretu i K1 otwartego z niej; false = zwykła mapa (src/nawigacja.h).
  bool przejsciePortretu(Ekran przed, Przycisk p, bool dlugo, const KontekstNaw& kn, Przejscie& pr) {
    if (przed == Ekran::DZIEN && r.portretStrona && sekundaDzis(r.slotDziennyEp) < 0 && !kn.nocny && !kn.pytanieTN &&
        !dlugo && p == Przycisk::PRAWY) {
      pr = {Ekran::PORTRET, Akcja::POKAZ};
      return true;
    }
    if (przed == Ekran::PORTRET) {
      pr = {Ekran::PORTRET, Akcja::NIC};
      if (dlugo) return true;
      if (p == Przycisk::PRAWY) {
        r.k1ZPortretu = 1;
        pr = {Ekran::K1, Akcja::POKAZ};
      } else if (p == Przycisk::LEWY || p == Przycisk::SRODEK) {
        pr = {Ekran::DZIEN, Akcja::POKAZ};
      }
      return true;
    }
    if (przed == Ekran::K1 && r.k1ZPortretu && !dlugo && p == Przycisk::LEWY && r.portretStrona) {
      pr = {Ekran::PORTRET, Akcja::POKAZ};
      return true;
    }
    if (przed != Ekran::K1 || p != Przycisk::PRAWY) r.k1ZPortretu = 0;  // K1 → K2 i dalej: ◀ z K1 wraca zwykle
    return false;
  }
  enum class ZPlikow : uint8_t { NOC, ZAPAS, PRZERWANE };
  // Noc doby `day` z plików sesji (ADR-0036, R-DANE-42…R-DANE-45): lista sesji doby z `/api/sessions`, wszystkie
  // zamknięte → pobranie PLD, EVE, CSL każdej sesji jeden po drugim (plik w pamięci tylko na czas odczytu), STR.edf
  // do ustawień, rekord nocy liczony na panelu. Każdy problem (AirBridge starszy niż 2.3.1, 503, doba bez sesji, sesja
  // niezamknięta albo `.part`, plik zniknął, ponad limit, nieczytelny, noc częściowa, za mało czasu) = ZAPAS: ta doba
  // z raportu JSON jak dotąd, w tym samym wybudzeniu. 404 i błąd listy wyłączają pliki do końca wybudzenia.
  // `przyczyna` (może być nullptr): przy ZAPAS przyczyna dla trybu porównawczego (src/porownanie.h).
  // `rezerwaMs`: najmniej tyle budżetu żądań przed każdym żądaniem (lista, STR, plik), inaczej ZAPAS (`brak czasu`).
  ZPlikow dobaZPlikow(long day, NocAB& wynik, porownanie::Przyczyna* przyczyna = nullptr,
                      long rezerwaMs = (long)AB_LIMIT_ODPOWIEDZI_MS) {
    using namespace noc_pliki;
    using P = porownanie::Przyczyna;
    P zastepczy = P::BRAK;
    P& pw = przyczyna ? *przyczyna : zastepczy;
    pw = P::BRAK;
    if (sesjeAB == 0) {
      pw = sesjeABPowod;
      return ZPlikow::ZAPAS;
    }
    std::string data = dataPoranka(day), cel = dzienAB(day), kursor;
    std::vector<SesjaAB> sesje;
    for (int strona = 0;; strona++) {
      if (abZostalo() < rezerwaMs) {
        log("noc %s z plików: za mało czasu w tym wybudzeniu, raport JSON", data.c_str());
        pw = P::BRAK_CZASU;
        return ZPlikow::ZAPAS;
      }
      std::string t;
      int kod = abGetRaz(sciezkaSesjiDoby(day, kursor), t);
      if (abPrzerwano) return ZPlikow::PRZERWANE;
      ListaSesji l;
      if (kod != 200 || !parsujListeSesji(t, l)) {
        sesjeAB = 0;
        sesjeABPowod = pw = kod == 404 ? P::STARY_AIRBRIDGE : P::BLAD_LISTY;
        if (kod == 404) log("noc z plików: /api/sessions 404 (AirBridge starszy niż 2.3.1), noce z raportu JSON");
        else log("noc z plików: /api/sessions bez listy (kod %d), noce z raportu JSON w tym wybudzeniu", kod);
        return ZPlikow::ZAPAS;
      }
      sesjeAB = 1;
      InfoStr is;
      if (parsujSesjeStr(t, is) && is.jest && !is.etag.empty()) strEtagListy = is.etag;
      bool dalej = !l.kursor.empty();
      for (auto& x : l.sesje) {
        if (x.day == cel) sesje.push_back(x);
        else if (x.day > cel) dalej = false;
      }
      if (!dalej) break;
      if (strona + 1 >= STRON_NA_DOBE) {
        log("noc %s z plików: ponad %d stron listy sesji, raport JSON", data.c_str(), STRON_NA_DOBE);
        pw = P::ZA_DUZO_SESJI;
        return ZPlikow::ZAPAS;
      }
      kursor = l.kursor;
    }
    if (sesje.empty()) {
      log("noc %s z plików: doba bez sesji na liście AirBridge, raport JSON", data.c_str());
      pw = P::BEZ_SESJI;
      return ZPlikow::ZAPAS;
    }
    LicznikNocy licz(day);
    std::map<std::string, std::string> sciezki;  // nazwa pliku → ścieżka na karcie AirBridge
    for (auto& x : sesje) {
      bool part = false;
      for (auto& f : x.pliki) {
        const std::string& p = f.sciezka;
        part = part || (p.size() >= 5 && p.compare(p.size() - 5, 5, ".part") == 0);
      }
      if (!x.closed || part) {
        log("noc %s z plików: sesja niezamknięta, raport JSON (noc w budowie albo częściowa jak dotąd)", data.c_str());
        pw = P::NIEZAMKNIETA;
        return ZPlikow::ZAPAS;
      }
      for (auto& f : x.pliki) {
        std::string nazwa = nazwaZeSciezki(f.sciezka);
        if (rodzajPliku(nazwa) == Rodzaj::INNY) continue;
        licz.dodajNazwe(nazwa);
        sciezki[nazwa] = f.sciezka;
      }
    }
    const Str* st = strDoNocy();
    if (!st) {
      log("noc %s z plików: bez STR.edf (ustawienia doby), raport JSON", data.c_str());
      pw = P::BEZ_STR;
      return ZPlikow::ZAPAS;
    }
    if (!st->doby.count(day)) {  // ADR-0036 pkt 4: bez doby w STR noc nie ma ustawień, raport JSON je ma
      log("noc %s z plików: STR.edf jeszcze bez tej doby (ustawienia), raport JSON", data.c_str());
      pw = P::STR_BEZ_DOBY;
      return ZPlikow::ZAPAS;
    }
    // Portret nocy (R-POR-17, T-WYM-98): odcinki 5 min w tym samym przejściu po próbkach, tylko dla doby, która jest
    // ostatnią nocą w historii (doba starsza nie nadpisuje pliku); bez dodatkowych żądań do AirBridge
    const bool doPortretu = portretDoby(day);
    portret::Zbieracz zbieracz(day, portret::maksimumZeStr(*st, day));
    if (doPortretu) licz.dolaczPortret(&zbieracz);
    size_t bajty = 0;
    int plikow = 0;
    for (auto& nazwa : licz.kolejnosc(false, POBIERAJ_SAD)) {
      if (abZostalo() < rezerwaMs) {
        log("noc %s z plików: za mało czasu w tym wybudzeniu, raport JSON", data.c_str());
        pw = P::BRAK_CZASU;
        return ZPlikow::ZAPAS;
      }
      std::string t;
      int kod = abGetRaz(sciezkaPobraniaPliku(sciezki[nazwa]), t);
      if (abPrzerwano) return ZPlikow::PRZERWANE;
      if (kod != 200) {
        log("noc %s z plików: %s niepobrany (kod %d), raport JSON", data.c_str(), nazwa.c_str(), kod);
        pw = P::PLIK_NIEPOBRANY;
        return ZPlikow::ZAPAS;
      }
      bajty += t.size();
      plikow++;
      if (!licz.dodajPlik(nazwa, (const uint8_t*)t.data(), t.size())) {
        log("noc %s z plików: %s nieczytelny, raport JSON", data.c_str(), nazwa.c_str());
        pw = P::PLIK_NIECZYTELNY;
        return ZPlikow::ZAPAS;
      }
    }
    WynikNocy w = licz.wynik();
    if (w.czesciowa) {
      std::string p;
      for (auto& x : w.problemy) p += (p.empty() ? "" : "; ") + x;
      log("noc %s z plików: częściowa (%s), raport JSON", data.c_str(), p.c_str());
      pw = P::CZESCIOWA;
      return ZPlikow::ZAPAS;
    }
    wynik = nocZWyniku(w, st, r.ab.tryb, abPna, abSrn);
    if (doPortretu) zapiszPortret(zbieracz.wynik(w, licz.zdarzeniaPortretu()), zbieracz.bajty());
    log("noc %s z plików sesji: %d sesji, %d plików, %u B, liczniki ok. %u B", data.c_str(), (int)w.sesje.size(),
        plikow, (unsigned)bajty, (unsigned)licz.bajtyLicznikow());
    return ZPlikow::NOC;
  }
  // ---------- tryb porównawczy (`noc_zrodlo=porownanie`, tylko jawnie; ADR-0036 „Zmiana 2026-11-09” i „2026-11-11”) ----------
  static bool trybPorownania(const UstawieniaDnia& u) { return u.nocZrodloSkuteczne() == 2; }
  porownanie::Zrodlo zrodloNocy() {
    int z = ustawieniaB9().nocZrodloSkuteczne();
    return z == 1 ? porownanie::Zrodlo::PLIK : z == 0 ? porownanie::Zrodlo::JSON : porownanie::Zrodlo::POROWNANIE;
  }
  std::vector<porownanie::Wpis>& wpisyPorownania() {
    if (!porWczytany) {
      porWczytany = true;
      porWpisy = porownanie::parsujWpisy(wczytaj("porownanie"));
    }
    return porWpisy;
  }
  // Doba z kompletnym raportem JSON w trybie porównawczym: noc idzie do historii z raportu (bez zmian), a porównanie
  // z nocą z plików czeka na koniec wybudzenia (porownanieNaKoniec(): po raportach wszystkich dób, zapisie historii,
  // rysowaniu i czuwaniu), więc ekran poranka, zapis nocy, zapisy plików i budżet zaległych dób są takie jak przy
  // `json`. Tu tylko pamięć: noc z raportu do porównania bez drugiego żądania; wpis `czeka` w pliku dopiero na końcu.
  void zaplanujPorownanie(long day, const NocAB& zJson) {
    if (!porownanie::gotowe(wpisyPorownania(), dataPoranka(day))) porNoce[day] = zJson;
  }
  // Doba do porównania na końcu wybudzenia: najnowsza z wpisem `brak` i przyczyną ponawianą (czekaPonowienia())
  // z ostatnich DOB_PONAWIANIA dób; -1 = brak.
  long dobaDoPorownania() {
    using namespace porownanie;
    std::string dzis = dzisiaj();
    if (dzis.empty()) return -1;
    const std::vector<Wpis>& w = wpisyPorownania();
    for (auto it = w.rbegin(); it != w.rend(); ++it)
      if (it->werdykt == Werdykt::BRAK && czekaPonowienia(it->przyczyna) &&
          nrDnia(it->data) > nrDnia(dzis) - DOB_PONAWIANIA && nrDnia(it->data) <= nrDnia(dzis))
        return nrDnia(it->data) - 1;  // data poranka → numer doby terapii
    return -1;
  }
  // Tryb porównawczy na samym końcu wybudzenia (po rysowaniu i czuwaniu, przed snem): najwyżej POROWNAN_NA_WYBUDZENIE
  // dób (najnowsza), tylko z budżetu żądań, który ZOSTAŁ po raportach i zaległych dobach (abKoniecMs bez zmian).
  // Mniej niż CZAS_POROWNANIA_MS = wpis `brak czasu` bez Wi-Fi (ponowienie w kolejnym wybudzeniu); w trakcie każde
  // żądanie dalej wymaga AB_LIMIT_ODPOWIEDZI_MS (dobaZPlikow(), rezerwa), więc żadne nie wyjdzie poza budżet.
  // Wi-Fi włączane na nowo (wyłączone przed rysowaniem jak przy `json`), przerywalne przyciskiem (bez wpisu), zapis
  // /dane/porownanie.csv po wyłączeniu Wi-Fi. Ekrany pacjenta, historia i NVS bez zmian.
  void porownanieNaKoniec(PowodWybudzenia pw) {
    using namespace porownanie;
    if (!trybPorownania(ustawieniaB9())) return;
    std::vector<Wpis>& w = wpisyPorownania();
    // doby z raportem JSON z tego wybudzenia: wpis `czeka` (ponowienie w kolejnych wybudzeniach, gdy nie teraz)
    for (auto& kv : porNoce) {
      std::string d = dataPoranka(kv.first);
      bool juzCzeka = false;
      for (auto& x : w) juzCzeka = juzCzeka || (x.data == d && x.werdykt == Werdykt::BRAK && czekaPonowienia(x.przyczyna));
      if (!juzCzeka && !gotowe(w, d)) {
        dopisz(w, wpisBraku(d, Przyczyna::CZEKA));
        porZm = true;
      }
    }
    zapiszPorownanie();  // Wi-Fi wyłączone: zapis od razu (tylko przy zmianie)
    if (pw == PowodWybudzenia::PRZYCISK || przyciskWCzuwaniu) return;
    if (k.pierwszaKonfiguracja || !wifiByloPolaczone || !odczytUdany || r.ab.zablokowane || otaAktywne ||
        ekran() == Ekran::TEST || ap.aktywna || !zapisB5())
      return;
    // plik z nowszej wersji (tylko do odczytu, ADR-0022 pkt 4): wynik nie byłby zapisany, więc bez pobrań
    if (bezZapisu.count("porownanie") || mag.tylkoDoOdczytu(mag.sciezkaRoku("porownanie", ""))) return;
    long day = dobaDoPorownania();
    if (day < 0) return;
    std::string data = dataPoranka(day);
    for (int n = 0; n < POROWNAN_NA_WYBUDZENIE && day >= 0; n++) {
      Wpis e;
      if (abZostalo() < (long)CZAS_POROWNANIA_MS) {
        log("porównanie nocy %s: za mało budżetu po raportach (%ld ms), ponowienie w kolejnym wybudzeniu", data.c_str(),
            abZostalo());
        e = wpisBraku(data, Przyczyna::BRAK_CZASU);
      } else {
        if (!a.siec.polaczona()) {
          if (!wifiOdMs) wifiOdMs = ms() ? ms() : 1;
          wifiProbowano = true;
          bool przerwane = false;
          abPrzerywanie(true);
          bool jest = a.siec.polacz(LIMIT_WIFI_MS, [this]() {
            odnow();
            return abPrzerwij();
          }, przerwane);
          odlozZapis = true;
          if (!jest || przerwane) {
            log("porównanie nocy %s: bez Wi-Fi%s, ponowienie w kolejnym wybudzeniu", data.c_str(),
                przerwane ? " (przycisk)" : "");
            abPrzerywanie(false);
            zamknijSiec();
            return;
          }
        }
        odlozZapis = true;
        abPrzerywanie(true);
        NocAB zJson;
        bool jest = porNoce.count(day) > 0;
        if (jest) {
          zJson = porNoce[day];
        } else {  // doba z wcześniejszego wybudzenia: raport JSON jeszcze raz (pamięć tego wybudzenia go nie ma)
          std::string t;
          int kod = abGetRaz(sciezkaNocy(day), t);
          if (kod == 200) zJson = nocZRaportu(t, day, r.ab.tryb, abPna, abSrn);
          jest = kod == 200 && zJson.stan == StanRaportu::KOMPLETNA;
          if (!jest) log("porównanie nocy %s: raport JSON niedostępny (kod %d), ponowienie później", data.c_str(), kod);
        }
        if (abPrzerwano) {
          log("porównanie nocy %s: przerwane przyciskiem, bez wpisu", data.c_str());
          abPrzerywanie(false);
          zamknijSiec();
          return;
        }
        if (!jest) {
          abPrzerywanie(false);
          zamknijSiec();
          return;
        }
        NocAB p;
        Przyczyna przy = Przyczyna::BRAK;
        ZPlikow z = dobaZPlikow(day, p, &przy, (long)AB_LIMIT_ODPOWIEDZI_MS);
        abPrzerywanie(false);
        if (z == ZPlikow::PRZERWANE) {
          log("porównanie nocy %s: przerwane przyciskiem, bez wpisu", data.c_str());
          zamknijSiec();
          return;
        }
        e = z == ZPlikow::NOC ? wpisZWyniku(data, porownaj(p, zJson)) : wpisBraku(data, przy);
      }
      dopisz(w, e);
      porZm = true;
      log("porównanie nocy %s z plików z raportem JSON: %s%s%s", data.c_str(), nazwaWerdyktu(e.werdykt),
          e.werdykt == Werdykt::BRAK ? (" (" + e.przyczyna + ")").c_str() : "",
          e.rozjazd.empty() ? "" : (" " + e.rozjazd).c_str());
      day = -1;  // POROWNAN_NA_WYBUDZENIE = 1: kolejne doby w następnych wybudzeniach
    }
    if (a.siec.polaczona()) zamknijSiec();
    else zapiszPorownanie();
  }
  // Historia nocy ze STR.edf (ADR-0035, R-DANE-38…R-DANE-41): w wybudzeniu z listą nocy, najwyżej raz. `/api/sessions`
  // daje etag STR; inny niż zapamiętany (albo nieznany) → pobranie całego pliku, odczyt na panelu, noce 'S' do historii
  // (wpiszNocS()), etag po zapisie historii. 404 = AirBridge bez `/api/sessions` (starszy niż 2.3.1); inny błąd = próba
  // w następnym takim wybudzeniu. Żaden wynik nie zmienia httpKod (bez E-01, E-03 i linii na ekranie).
  void strZAirBridge(StanHistAB& s, const std::string& dzis) {
    if (strProbowano || dzis.empty() || !fsOk() || bezZapisu.count("noce")) return;
    strProbowano = true;
    if (abZostalo() < 2 * (long)AB_LIMIT_ODPOWIEDZI_MS) {
      log("STR: za mało czasu w tym wybudzeniu, próba w następnym");
      return;
    }
    std::string t;
    int kod = 0;
    InfoStr in;
    // STR pobrany już w tym wybudzeniu do nocy z plików (strDoNocy()): etag z listy sesji odczytanej PRZED tym
    // pobraniem (etag nie nowszy niż treść, którą przetwarzamy); bez etagu noce 'S' z tego pliku, etag bez zapisu
    const bool zPlikuNocy = strWybOk;
    if (zPlikuNocy) {
      in.jest = true;
      in.etag = strWybEtag;
    } else {
      kod = abGetRaz(sciezkaSesjiStr(), t);
      if (kod == 404) {
        log("STR: /api/sessions 404 (AirBridge starszy niż 2.3.1), historia ze STR niedostępna");
        return;
      }
      if (kod != 200) {
        log("STR: /api/sessions bez odpowiedzi (kod %d), próba w następnym wybudzeniu z listą nocy", kod);
        return;
      }
      if (!parsujSesjeStr(t, in)) {
        log("STR: /api/sessions nieczytelne, próba w następnym wybudzeniu z listą nocy");
        return;
      }
      if (!in.jest || in.etag.empty()) {
        log("STR: AirBridge nie podaje pliku STR (str null albo bez etag)");
        return;
      }
    }
    // zapis w NVS: `{etag}|{liczba nocy 'S' w historii po przetworzeniu}`; etag znany, ale bez żadnej nocy 'S'
    // w historii, choć plik je dawał (pliki nocy zgubione albo odtworzone bez nich) = etag nieznany, plik od nowa
    std::string zapamietany = a.nvs.tekst("cpap", NVS_KLUCZ_STR_ETAG, "");
    int nocyZapisane = -1;  // -1 = nieznana liczba (zapis bez liczby)
    size_t kreska = zapamietany.rfind('|');
    if (kreska != std::string::npos && kreska + 1 < zapamietany.size() &&
        zapamietany.find_first_not_of("0123456789", kreska + 1) == std::string::npos && zapamietany.size() - kreska <= 7) {
      nocyZapisane = (int)std::strtol(zapamietany.c_str() + kreska + 1, nullptr, 10);  // same cyfry, ≤ 6 znaków
      zapamietany.resize(kreska);
    }
    int nocySTeraz = 0;
    for (auto& n : hist) nocySTeraz += n.zrodlo == 'S';
    if (!in.etag.empty() && zapamietany == in.etag && (nocySTeraz > 0 || nocyZapisane == 0)) {
      log("STR: bez zmian (etag %s)", in.etag.c_str());
      return;
    }
    if (!in.etag.empty() && zapamietany == in.etag)
      log("STR: etag bez zmian, ale w historii nie ma nocy ze STR: pobieram plik od nowa");
    if (!strWybOk) {  // plik pobrany już w tym wybudzeniu do nocy z plików sesji (strDoNocy()) nie jest pobierany drugi raz
      kod = abGetRaz(sciezkaPobraniaStr(), t);
      if (kod != 200) {
        log("STR: pobranie nieudane (kod %d), próba w następnym wybudzeniu z listą nocy", kod);
        return;
      }
      size_t rozmiarPliku = t.size();
      Str nowy;
      edf::Blad b = czytajStr(std::move(t), nowy);  // bajty pliku przechodzą do `nowy` bez kopii
      if (b != edf::Blad::BRAK) {
        log("STR: plik nieczytelny (%s, %u B), próba w następnym wybudzeniu z listą nocy", edf::opisBledu(b),
            (unsigned)rozmiarPliku);
        return;
      }
      strWyb = std::move(nowy);
      strWybOk = true;
    }
    const Str& st = strWyb;
    size_t rozmiar = st.bajty.size();
    std::vector<Noc> noce = noceZeStr(st, dzis, abPna, abSrn);
    int zmienione = 0;
    for (auto& n : noce)
      if (wpiszNocS(hist, n)) zmienione++;
    if (zmienione) s.histZm = true;
    int nocyS = 0;
    for (auto& n : hist) nocyS += n.zrodlo == 'S';
    if (!in.etag.empty()) strEtagCzeka = in.etag + "|" + std::to_string(nocyS);
    else log("STR: plik z nocy z plików bez etagu z listy sesji, etag bez zapisu (pobranie od nowa w następnym wybudzeniu)");
    log("STR: %u B, %d dób, %d nocy z terapią, %d wpisanych do historii (etag %s)", (unsigned)rozmiar,
        (int)st.doby.size(), (int)noce.size(), zmienione, in.etag.c_str());
  }
  bool staleNaLiscie(long day) {
    for (auto& x : abLista)
      if (x.day == day) return x.stale;
    return false;
  }
  void nieudanaProba(long day, StanHistAB& s, const char* co) {
    if (abProbowane.insert(day).second) {
      zapiszNieudanaProbe(s.proby, day, teraz());
      s.probyZm = true;
    }
    log("doba %ld (noc %s): %s, próba %d z %d", day, dataPoranka(day).c_str(), co,
        s.proby.count(day) ? s.proby[day].proby : 0, AB_PROB_DOBY);
  }

  // Jedna doba z AirBridge.
  bool raportCzesciowyOstatniej = false;  // raport ostatniej nocy w tym wybudzeniu miał `partial = true`
  WynikDoby dobaZAirBridge(long day, const std::string& dzis, bool spr11, StanHistAB& s, int& kodBledu) {
    std::string data = dataPoranka(day);
    bool ostatnia = data == dzis;
    bool ponownie = spr11 && ostatnia;  // R-DANE-24
    bool stale = staleNaLiscie(day);
    // ADR-0036: `noc_zrodlo=plik` (i bez wiersza, „Zmiana 2026-11-11”) → noc z plików sesji liczona na panelu, raport
    // JSON tylko jako zapas; z listą
    // `noc_json` najpierw raport JSON (jak dotąd), potem pliki, a wymienione metryki zostają z raportu
    const UstawieniaDnia& ust = ustawieniaB9();
    // zła lista `noc_json` (choć jedno nieznane ID): bezpieczny kierunek, cała noc z raportu JSON
    const bool zPlikowUst = ust.nocZrodloSkuteczne() == 1 && !ust.nocJsonZly;
    NocAB n;
    bool zPlikow = false;
    kodBledu = 200;
    if (zPlikowUst && ust.nocJson.empty()) {
      ZPlikow z = dobaZPlikow(day, n);
      if (z == ZPlikow::PRZERWANE) return WynikDoby::PRZERWANE;
      zPlikow = z == ZPlikow::NOC;
    }
    if (!zPlikow) {
      std::string tresc;
      int kod;
      bool przerwane = false;
      Odpowiedz o = abRaport(sciezkaNocy(day), tresc, AB_LIMIT_JOBA_MS, kod, &przerwane);
      kodBledu = kod;
      if (przerwane) return WynikDoby::PRZERWANE;
      if (o == Odpowiedz::BLAD && (kod == 401 || (kod <= 0 && kod != AB_KOD_URWANE && kod != AB_KOD_ZA_DUZO))) {
        if (ponownie) nvsTekst("ab_11", dzis);  // R-DANE-24 fail-path
        return WynikDoby::BRAK_POLACZENIA;
      }
      if (o == Odpowiedz::W_BUDOWIE) {  // R-AW-15
        if (!ostatnia) nieudanaProba(day, s, "raport w budowie po limicie");
        return WynikDoby::CZEKA;
      }
      if (o == Odpowiedz::BLAD) {  // R-AW-12
        nieudanaProba(day, s, "odpowiedź bez raportu");
        if (ponownie) nvsTekst("ab_11", dzis);
        return WynikDoby::NIEUDANA;
      }
      n = nocZRaportu(tresc, day, r.ab.tryb, abPna, abSrn);  // status tego wybudzenia
      tresc.clear();
      tresc.shrink_to_fit();
      // tryb porównawczy: noc zostaje z raportu JSON, noc z plików tylko do porównania na końcu wybudzenia
      if (trybPorownania(ust) && n.stan == StanRaportu::KOMPLETNA) zaplanujPorownanie(day, n);
      if (zPlikowUst && !ust.nocJson.empty() && n.stan == StanRaportu::KOMPLETNA) {
        NocAB p;
        ZPlikow z = dobaZPlikow(day, p);
        if (z == ZPlikow::PRZERWANE) return WynikDoby::PRZERWANE;
        if (z == ZPlikow::NOC) {
          noc_pliki::scalZJson(p, n, ust.nocJson);
          n = p;
          zPlikow = true;
          log("noc %s: z plików sesji, %d metryk z raportu JSON (noc_json)", data.c_str(), (int)ust.nocJson.size());
        }
      }
    }
    if (stale && !s.proby[day].staleZrobione) {
      s.proby[day].staleZrobione = true;
      s.probyZm = true;
    }
    int proby = (s.proby.count(day) ? s.proby[day].proby : 0) + (abProbowane.count(day) ? 0 : 1);
    PoRaporcie co = poRaporcie(n.stan, data, dzis, minutaDoby(), proby);
    if (ostatnia && n.stan == StanRaportu::CZESCIOWA) {
      // R-DANE-25: noc, która weszła w budowę po 11:00, dostaje rekord „częściowa” dopiero o 23:30; wcześniej
      // czeka (sprawdzenie co 30 min, R-DANE-18). Wejście liczone ze stanu nocy z tą obserwacją.
      raportCzesciowyOstatniej = true;
      stan_nocy::FazaNocy po = stan_nocy::przejdz(fazaNocy(), dzis, minutaDoby(), stan_nocy::Obserwacja::RAPORT_CZESCIOWY,
                                                  nocZDnia(hist, dzis));
      if (co == PoRaporcie::ZAPISZ && !nocZDnia(hist, dzis) && !stan_nocy::czesciowaTeraz(po, dzis, minutaDoby())) {
        log("noc %s: partial po wejściu w budowę po 11:00, rekord „częściowa” o 23:30 (R-DANE-25)", data.c_str());
        co = PoRaporcie::CZEKAJ;
      }
    }
    switch (co) {
      case PoRaporcie::ZAPISZ: {
        bool zostawiony = false;
        const Noc* przed = nocZDnia(hist, data);
        uint32_t importPrzed = przed ? przed->import : 0;  // R1 karty chwila pierwszego zapisu kompletnego zostaje
        if (wpiszNocAB(hist, n.noc, ponownie || stale, n.pelna, &zostawiony)) {
          for (auto& x : hist)
            if (x.data == data) x.import = importPrzed ? importPrzed : (!x.czesciowa && zegar() ? teraz() : 0);
          s.histZm = true;
          log("noc %s z AirBridge%s%s%s: %s", data.c_str(), zPlikow ? " (pliki sesji)" : "",
              n.pelna ? "" : " (bez kanałów EDF)",
              n.noc.czesciowa ? " (częściowa)" : "", zapiszNoc(n.noc).c_str());
          if (ostatnia && !n.noc.czesciowa && minutaDoby() < 11 * 60 && !ponownie) nvsTekst("ab_rano", dzis);
          // R-KON-41: chwila importu ostatniej nocy (pierwszy zapis rekordu kompletnego tej doby) do planu
          if (ostatnia && !n.noc.czesciowa && zegar() && sekundaDzis(r.importNocyEp) < 0) r.importNocyEp = teraz();
          // D52: chwila ostatniego udanego pobrania nocy (U-05 `Ostatnie pobranie`, T-WYM-39), także przy ponownym
          // pobraniu nocy, której `import` zostaje z pierwszego zapisu
          if (!n.noc.czesciowa && zegar()) a.nvs.ustawLiczbeU("cpap", NVS_KLUCZ_POBRANIE_NOCY, teraz());
        }
        if (n.noc.czesciowa) {  // R-DANE-15, R-DANE-25
          if (!ostatnia) nieudanaProba(day, s, "noc częściowa (partial)");
          return WynikDoby::CZEKA;
        }
        if (zostawiony) {
          log("doba %ld (noc %s): bez kanałów EDF, zostaje pełniejszy rekord", day, data.c_str());
          s.pomZm = s.pom.insert(day).second || s.pomZm;
        }
        if (s.proby.count(day) && s.proby[day].proby) {
          s.proby[day].proby = 0;
          s.probyZm = true;
        }
        if (ponownie) nvsTekst("ab_11", dzis);
        return WynikDoby::ZAPISANA;
      }
      case PoRaporcie::CZEKAJ: log("noc %s: partial, czekam", data.c_str()); return WynikDoby::CZEKA;
      case PoRaporcie::PONOW:
        nieudanaProba(day, s, n.stan == StanRaportu::ZLY_FORMAT ? "raport bez klucza krytycznego" : "raport nieczytelny");
        if (ponownie) nvsTekst("ab_11", dzis);
        return WynikDoby::NIEUDANA;
      case PoRaporcie::POMIN:
        nieudanaProba(day, s, "raport nieczytelny");
        log("doba %ld (noc %s): %d nieudanych prób, pomijam na stałe", day, data.c_str(), proby);
        s.pomZm = s.pom.insert(day).second || s.pomZm;
        return WynikDoby::NIEUDANA;
      case PoRaporcie::E03:
        if (n.stan == StanRaportu::NIECZYTELNY) nieudanaProba(day, s, "raport nieczytelny");
        if (ponownie) nvsTekst("ab_11", dzis);
        log("noc %s: raport nieczytelny (E-03)", data.c_str());
        return WynikDoby::E03;
    }
    return WynikDoby::NIEUDANA;
  }

  // R-AW-18: drugie źródło czasu. Wołane tylko przy zegarze nieznanym (zegar znany zostaje, R-AW-19) po tym,
  // jak 3 próby NTP zawiodły. `esp_time` to czas lokalny w strefie panelu (AirBridge skonfigurowany na Europe/Warsaw;
  // korekta o przesunięcie z T-WYM-80 poza prototypem, więc przesunięcie 0). Odrzuca "--", pustą i nieczytelną wartość
  // oraz czas nie późniejszy niż CZAS_ZNANY_OD. To nie synchronizacja NTP: `ntpOstatnio` bez zmian, więc NTP wraca
  // w kolejnym wybudzeniu zgodnie z R-AW-08.
  void zegarZAirBridge(const StatusAB& st) {
    std::tm lok{};
    if (!parsujCzasAb(st.czas, lok)) {
      log("czas z AirBridge: pole esp_time %s", st.czas.empty() ? "brak" : "nieczytelne albo \"--\"");
      return;
    }
    uint32_t t = a.zegar.zLokalnego(lok);
    if (!zegarZnany(t)) {
      log("czas z AirBridge: %s odrzucony (przed %lu)", st.czas.c_str(), (unsigned long)CZAS_ZNANY_OD);
      return;
    }
    if (!a.zegar.ustaw(t) || !zegar()) {
      log("czas z AirBridge: nie udało się ustawić zegara");
      return;
    }
    log("źródło czasu: AirBridge (%s)", st.czas.c_str());
  }

  // Wynik: 1 udany odczyt, 0 nieudany, -1 przerwany przyciskiem.
  int pobierz(bool przerywalny) {
    abPobierano = true;
    obsNocy = stan_nocy::Obserwacja::ZEGAR;
    abPrzerywanie(przerywalny);
    if (!wifiOdMs) wifiOdMs = ms() ? ms() : 1;  // pomiar baterii: czas z włączonym Wi-Fi do zaśnięcia
    bool przerwanePolaczenie = false;
    wifiProbowano = true;
    bool jest = a.siec.polacz(LIMIT_WIFI_MS, [this]() {
      odnow();
      return abPrzerwij();
    }, przerwanePolaczenie);
    odlozZapis = true;  // zapis plików pobrania po wyłączeniu Wi-Fi (zamknijSiec())
    if (przerwanePolaczenie) return -1;
    if (!jest) {
      httpKod = KOD_WIFI;
      wifiOpis = a.siec.opisBledu();  // adapter wypisuje status, powód rozłączenia i skan sieci
      log("Wi-Fi: brak (%s)", wifiOpis.c_str());
      return 0;
    }
    wifiByloPolaczone = true;
    odnow();
    bool ntp = false;
    if (zegar()) {
      ntp = a.siec.czasZSieci(LIMIT_NTP_MS);  // ADR-0013
    } else {  // (R-AW-18): zegar nieznany, więc pierwsza synchronizacja dostaje 3 dłuższe próby
      for (int proba = 1; proba <= NTP_PROBY_BEZ_ZEGARA && !ntp; proba++) {
        ntp = a.siec.czasZSieci(LIMIT_NTP_PROBY_BEZ_ZEGARA_MS);
        odnow();
        if (!ntp) log("NTP: próba %d z %d bez odpowiedzi", proba, NTP_PROBY_BEZ_ZEGARA);
      }
    }
    odnow();
    if (ntp) r.ntpOstatnio = teraz();
    zapamietajWifi();  // U-05 bez połączenia: ostatnie połączenie z godziną (R-KONF-41)
    std::string ostatnio = r.ntpOstatnio ? dataZ(r.ntpOstatnio) + " " + godzina(r.ntpOstatnio) : "brak";
    log("NTP: %s, ostatnia synchronizacja %s", ntp ? "zsynchronizowany" : "brak odpowiedzi, zegar bez korekty",
        ostatnio.c_str());
    abAdres = k.abAdres;
    abKoniecMs = ms() + AB_LIMIT_WYBUDZENIA_MS;

    // 1. status: wersja, model, tryb, rop, edf_prefix (R-AW-01: do 3 prób)
    std::string tresc;
    abListaOk = false;
    // ocena slotu dziennego jedną próbą z krótkim limitem; brak odpowiedzi to zwykła ocena
    // (R-KON-56), bez ponowień R-AW-01 (milczący AirBridge dawał ok. 66 s Wi-Fi na ocenę)
    int kodStatusu = tylkoStatusTeraz ? abGetRaz("/api/status", tresc, LIMIT_STATUSU_SLOTU_MS)
                                      : abGet("/api/status", tresc);
    if (abPrzerwano) return -1;
    httpKod = kodStatusu;
    StatusAB st;
    PoStatusie ps = poStatusie(kodStatusu, tresc, st);
    // R-DANE-62: chwila odczytu z przetwarzaniem plików po terapii (powtórka po 2 min); każdy inny wynik ją kasuje
    r.przetwarzaEp = ps == PoStatusie::PRZETWARZA && zegar() ? teraz() : 0;
    if (ps == PoStatusie::BRAK_ODPOWIEDZI) {  // R-AW-12; 401 = złe hasło
      // (R-CZY-19): ostatnia próba bez odpowiedzi AirBridge przy działającym panelu i Wi-Fi, w NVS
      // `cpap`/`ab_bl` najwyżej raz na godzinę (ADR-0022); panel bez zasilania nie zapisuje prób, więc nie „wyjazd”
      uint32_t bl = a.nvs.liczbaU("cpap", "ab_bl", 0);
      if (zegar() && teraz() >= bl + 3600) a.nvs.ustawLiczbeU("cpap", "ab_bl", teraz());
      return 0;
    }
    // E-03 z nieczytelnego `/api/status` zostawia stan z poprzedniego odczytu; E-03 z wersji spoza zakresu ma już
    // pola statusu (wersja na S5), jak w prototypie. Status czytelny ⇔ parsujStatus() dał pola (wersja niepusta).
    bool czytelny = statusCzytelny(ps, st);
    if (czytelny && !zegar()) zegarZAirBridge(st);  // R-AW-18: NTP nie odpowiedziało, a panel nie ma zegara
    if (czytelny) {
      kopiuj(r.ab.wersja, sizeof r.ab.wersja, st.wersja);
      kopiuj(r.ab.tryb, sizeof r.ab.tryb, st.tryb);
      kopiuj(r.ab.prefiks, sizeof r.ab.prefiks, st.prefiks);
      r.ab.rop = (int8_t)st.rop;
      r.ab.zablokowane = raportyZablokowane(st);
      abPna = st.model;  // (R-NOR-25): do rekordów nocy pobranych w tym wybudzeniu
      abSrn = st.seryjny;
    }
    if (ps == PoStatusie::E03) {
      if (czytelny) log("AirBridge %s: wersja spoza zakresu (od v2.2.0)", st.wersja.c_str());
      httpKod = KOD_E03;
      return 0;
    }
    r.ab.magia = MAGIA_AB;
    // (R-KON-42): sam odczyt stanu aparatu nie jest pobraniem danych, więc nie przesuwa godziny `dane {hh:mm}`
    // (R-EKR-04); bez wcześniejszego odczytu zapisuje chwilę jak zwykły odczyt
    abPoprzedni = r.ab.ma && zegarZnany(r.ab.pobrano) ? r.ab.pobrano : 0;  // przerwa AirBridge (R-CZY-19)
    if (!tylkoStatusTeraz || !r.ab.ma || !zegarZnany(r.ab.pobrano)) r.ab.pobrano = zegar() ? teraz() : 0;
    r.ab.ma = true;
    log("AirBridge %s, %s, tryb %s, system %s, rop %d, edf_prefix '%s', raport %s %s", st.wersja.c_str(),
        st.model.c_str(), st.tryb.c_str(), st.system.c_str(), st.rop, st.prefiks.c_str(), st.raport.c_str(),
        st.bladRaportu.c_str());

    if (ps == PoStatusie::ZABLOKOWANE || ps == PoStatusie::PRZETWARZA) obsNocy = stan_nocy::Obserwacja::STATUS_BUDOWA;
    if (wieczorTeraz) {  // R-KON-01: ocena slotu wieczornego, Wi-Fi tylko na `/api/status` i źródło otoczenia
      log("slot wieczorny: tylko odczyt stanu aparatu (rop %d)", st.rop);
      zapiszStanAB();
      return 1;
    }
    if (tylkoStatusTeraz) {  // R-KON-42: Wi-Fi tylko na `/api/status` (bez listy, raportów i zaległych dób)
      log("slot dzienny: tylko odczyt stanu aparatu (rop %d)", st.rop);
      drzemkaB9(st);  // R-KON-57, raport doby po drzemce w tej samej sesji Wi-Fi
      zapiszStanAB();
      return 1;
    }
    importujJesliCzas();
    std::string dzis = dzisiaj();
    int m = minutaDoby();
    r.ab.wBudowie = false;
    // 2. terapia trwa albo moduł blokuje raporty: bez zapytań o raporty (5.1.4)
    if (ps == PoStatusie::ZABLOKOWANE) {
      r.ab.wBudowie = wBudowiePrzyBlokadzie(!dzis.empty(), m, nocZDnia(hist, dzis) != nullptr);
      log("AirBridge: terapia trwa albo raporty zablokowane, bez raportów w tym wybudzeniu");
      zapiszStanAB();
      return 1;
    }
    // 2a. moduł przetwarza pliki po terapii: jak w budowie
    if (ps == PoStatusie::PRZETWARZA) {
      r.ab.wBudowie = wBudowiePrzyPrzetwarzaniu(!dzis.empty(), nocZDnia(hist, dzis) != nullptr);
      log("AirBridge: przetwarza pliki po terapii (post-processing / storage), bez listy w tym wybudzeniu");
      zapiszStanAB();
      return 1;
    }

    // 3. lista nocy i raport ostatniej nocy (tylko, gdy czegoś brakuje); zaległe doby w tej samej sesji Wi-Fi
    std::string rano = nvsTekst("ab_rano"), spr = nvsTekst("ab_11");
    bool spr11 = sprawdzenie11(dzis, m, rano, spr);
    StanHistAB& s = wczytajStanHist();
    int gotowe;
    uint32_t czekajDo;
    wczytajZalegle(gotowe, czekajDo);
    r.ab.zalegle = gotowe;
    raportCzesciowyOstatniej = false;
    if (ZAPIS_NOCY_Z_AIRBRIDGE && fsOk() && !dzis.empty() &&
        stan_nocy::pobranieO2330(fazaNocy(), dzis, m, nocZDnia(hist, dzis))) {
      // R-DANE-25 i R-DANE-35: o 23:30 noc w budowie od wejścia po 11:00 (albo z dniem na liście przy statusie
      // z R-DANE-35) dostaje rekord „częściowa”; lista nocy w slocie nocnym nie jest pobierana, więc sam raport dnia
      int kodDoby;
      log("noc %s: 23:30, raport nocy w budowie (R-DANE-25)", dzis.c_str());
      WynikDoby w = dobaZAirBridge(nrDnia(dzis) - 1, dzis, false, s, kodDoby);
      if (w == WynikDoby::ZAPISANA) obsNocy = stan_nocy::Obserwacja::RAPORT_KOMPLETNY;
      zapiszStanHist(s);
      zapiszStanAB();
      if (w == WynikDoby::PRZERWANE) return -1;
      if (w == WynikDoby::BRAK_POLACZENIA) httpKod = kodPoNieudanej(kodDoby);
      return w == WynikDoby::BRAK_POLACZENIA ? 0 : 1;  // raport bez rozstrzygnięcia to nie brak odpowiedzi (E-01)
    } else if (ZAPIS_NOCY_Z_AIRBRIDGE && fsOk() && potrzebnaLista(hist, dzis, m, spr11, gotowe, czekajDo, teraz(), s.pom)) {
      int kod;
      bool przerwane = false;
      Odpowiedz o = abRaport(sciezkaListy(), tresc, AB_LIMIT_JOBA_MS, kod, &przerwane);
      if (przerwane) {
        zapiszStanHist(s);
        return -1;
      }
      if (o == Odpowiedz::BLAD) {  // lista urwana albo nieczytelna: nieudana próba, nie E-03
        httpKod = kodPoNieudanej(kod);
        zapiszStanAB();
        return 0;
      }
      if (o == Odpowiedz::W_BUDOWIE) {
        r.ab.wBudowie = !nocZDnia(hist, dzis);  // R-AW-15
      } else {
        bool listaOk = parsujListeNocy(tresc, abLista);
        tresc.clear();
        tresc.shrink_to_fit();
        if (!listaOk) {  // R-AW-02, E-03
          httpKod = KOD_E03;
          zapiszStanAB();
          return 0;
        }
        if (abLista.empty()) {  // pusta lista: nie „brak nocy”, nie S5
          // (5.1.4): status bez stanu z R-DANE-35 (sprawdzony wyżej), więc to lista bez dnia ostatniej nocy:
          // przed 11:00 noc „nieobecna” (S10), od 11:00 noc bez terapii (E-10, R-DANE-20), bez S10
          obsNocy = stan_nocy::Obserwacja::LISTA_BEZ_DNIA;
          r.ab.wBudowie = !nocZDnia(hist, dzis) && m < stan_nocy::GODZINA_11;
          log("AirBridge: lista nocy pusta (dnia ostatniej nocy nie ma na liście)");
          strZAirBridge(s, dzis);  // ADR-0035: historia ze STR także przy pustej liście
          zapiszStanHist(s);
          zapiszStanAB();
          return 1;
        }
        abListaOk = true;
        wyjazdB15(abLista);  // noce przerwy ≥ 2 doby z czynnikiem „spanie poza domem” (R-CZY-19)
        if (odswiezStale(s.proby, abLista)) s.probyZm = true;
        {  // (5.1.4): dzień ostatniej nocy na liście? Bez niego: noc „nieobecna”, od 11:00 bez terapii (E-10)
          long ost = nrDnia(dzis) - 1;
          bool naLiscie = false;
          for (auto& x : abLista) naLiscie = naLiscie || x.day == ost;
          if (!naLiscie) obsNocy = stan_nocy::Obserwacja::LISTA_BEZ_DNIA;
        }
        std::vector<long> plan = planPobrania(abLista, hist, s.pom, dzis, spr11, 1, &s.proby, teraz());
        if (!plan.empty() && dataPoranka(plan[0]) == dzis) {
          int kodDoby;
          WynikDoby w = dobaZAirBridge(plan[0], dzis, spr11, s, kodDoby);
          if (w == WynikDoby::PRZERWANE) {
            zapiszStanHist(s);
            zapiszStanAB();
            return -1;
          }
          if (w == WynikDoby::CZEKA) r.ab.wBudowie = !nocZDnia(hist, dzis) || nocZDnia(hist, dzis)->czesciowa;
          if (w == WynikDoby::ZAPISANA) obsNocy = stan_nocy::Obserwacja::RAPORT_KOMPLETNY;
          if (raportCzesciowyOstatniej) obsNocy = stan_nocy::Obserwacja::RAPORT_CZESCIOWY;
          else if (w == WynikDoby::E03) httpKod = KOD_E03;
          else if (w == WynikDoby::BRAK_POLACZENIA || w == WynikDoby::NIEUDANA) httpKod = kodPoNieudanej(kodDoby);
        }
        if (httpKod == 200) strZAirBridge(s, dzis);  // ADR-0035: po ostatniej nocy, w tej samej sesji Wi-Fi
        zapiszStanHist(s);
        aktualizujZalegle(s, dzis);
        if (httpKod != 200) {
          zapiszStanAB();
          return 0;
        }
      }
    }
    zapiszStanHist(s);
    zapiszStanAB();
    return httpKod == 200 ? 1 : 0;
  }

  // Zaległe doby partiami po rysowaniu (warunki: dociagacZalegle()).
  void dociagnijHistorie() {
    std::string dzis = dzisiaj();
    StanHistAB& s = wczytajStanHist();
    std::vector<long> plan = planZaleglych(
        planPobrania(abLista, hist, s.pom, dzis, false, ZALEGLE_NA_WYBUDZENIE + 1, &s.proby, teraz()), dzis);
    if (plan.empty()) return;
    log("historia z AirBridge: %d zaległych dób, w tym wybudzeniu najwyżej %d", r.ab.zalegle, (int)plan.size());
    abKoniecMs = ms() + AB_LIMIT_ZALEGLYCH_MS;
    abPrzerywanie(true);
    int zrobione = 0;
    for (long day : plan) {
      odnow();
      if (abPrzerwij() || !zaczacRaportZaleglej(abZostalo())) break;
      int kod;
      WynikDoby w = dobaZAirBridge(day, dzis, false, s, kod);
      if (w == WynikDoby::BRAK_POLACZENIA || w == WynikDoby::PRZERWANE) break;  // sieć albo przycisk: koniec partii
      if (w == WynikDoby::ZAPISANA) zrobione++;
    }
    abPrzerywanie(false);
    zapiszStanHist(s);
    aktualizujZalegle(s, dzis);
    zapiszStanAB();
    log("historia z AirBridge: pobrane %d, gotowe %d, czeka %d, pominięte razem %d", zrobione, r.ab.zalegle,
        r.ab.czekajace, (int)s.pom.size());
  }

  // Godzina założenia maski na ekran nocny.
  OdczytMaski odczytMaski() {
    OdczytMaski o;
    if (odczytUdany) o.startSesji = startZPrefiksu(r.ab.rop, r.ab.prefiks, dzisiaj(), minutaDoby());
    log("godzina maski: rop %d, edf_prefix '%s' → %d", r.ab.rop, r.ab.prefiks, o.startSesji);
    return o;
  }

  // ---------- lokalne źródło otoczenia (ADR-0019) ----------
  bool otoSkonfigurowane() { return !k.otoUrl.empty(); }
  // R-NF-27: źródło skonfigurowane z adresem w sieci lokalnej; inaczej bez żądania i bez tokenu
  bool otoUzywalne() { return otoSkonfigurowane() && adresWSieciLokalnej(k.otoUrl); }
  uint32_t otoAdres() { return otoSkonfigurowane() ? hashAdresu(k.otoUrl.c_str()) : 0; }
  // (5.8.8): ekrany CO2 i ocena wieczorna z Wi-Fi w tym egzemplarzu;
  // z adresem źródła włączone (bez trybu próbnego), bez źródła albo z `co2=nie` w /konfiguracja.txt wyłączone
  bool co2B17() { return co2Wlaczone(otoUzywalne(), ustawieniaB9().co2); }

  int otoGetRaz(std::string& tresc, uint32_t koniecMs) {
    tresc.clear();
    ZadanieHttp z;
    z.url = k.otoUrl;
    z.limitMs = OTO_LIMIT_ODPOWIEDZI_MS;
    if (!k.otoToken.empty()) z.naglowki.emplace_back("X-Token", k.otoToken);
    z.maksBajtow = OTO_MAX_BAJTOW;
    z.cialoTylko200 = true;
    z.koniecMs = koniecMs;
    z.przerwij = [this]() { return abPrzerwij(); };
    odnow();
    uint32_t t0 = ms();
    OdpowiedzHttp o = a.siec.get(z);
    odnow();
    int kod = o.kod;
    if (kod == 200) {
      if (o.zaDuzo) {
        tresc.assign(OTO_MAX_BAJTOW + 1, ' ');  // za duże: parser odrzuci (ZA_DUZE)
      } else if (o.zaDlugo || o.przerwane || abPrzerwano) {
        kod = OTO_KOD_CZAS;  // limit czasu wybudzenia albo przycisk: brak odpowiedzi
      } else {
        tresc = std::move(o.cialo);
        if (!cialoKompletne(o.contentLength, o.wynikZapisu, tresc.size())) {
          kod = AB_KOD_URWANE;
          tresc.clear();
        }
      }
    }
    log("otoczenie: HTTP %d w %lu ms, %u B", kod, (unsigned long)(ms() - t0), (unsigned)tresc.size());
    return kod;
  }

  void pobierzOtoczenie(bool przerywalny) {
    otoTeraz = false;
    if (!otoUzywalne() || !a.siec.polaczona()) return;
    abPrzerywanie(przerywalny);
    bool nocny = zegar() && slotNocny(minutaDoby());
    int maxProb = probOtoczenia((StanOto)r.oto.stan == StanOto::BRAK_ODPOWIEDZI, nocny);
    uint32_t start = ms(), koniec = start + (maxProb == 1 ? OTO_BUDZET_1_MS : OTO_BUDZET_MS);
    std::string tresc;
    int kod = 0;
    for (int proba = 1;; proba++) {
      kod = otoGetRaz(tresc, koniec);
      if (abPrzerwano || !ponowicOtoczenie(kod, proba, (long)(int32_t)(koniec - ms()), maxProb)) break;
      bool przerwane = false;
      for (uint32_t c = ms(); ms() - c < OTO_ODSTEP_PROB_MS;) {
        odnow();
        if (abPrzerwij()) {
          przerwane = true;
          break;
        }
        czekaj(OBIEG_CZUWANIA_MS);
      }
      if (przerwane) break;
    }
    bool przerwane = abPrzerwano;
    abPrzerywanie(false);
    if (przerwane) {
      log("otoczenie: przerwane przyciskiem, zostają poprzednie dane");
      return;
    }
    uint32_t ep = zegar() ? teraz() : 0;
    if (kod != 200) {
      nieudanaOtoczenia(r.oto, kod, ep);  // 3xx: przekierowanie, bez podążania i bez ponowień (R-NF-27, R-AW-25)
      log("otoczenie: nieudana próba (kod %d, prób najwyżej %d), zostają dane z %s", kod, maxProb,
          godzina(r.oto.pobrano).c_str());
      return;
    }
    StanOto st = parsujOtoczenie(tresc, ep, r.oto);
    otoTeraz = st == StanOto::OK;
    if (st == StanOto::ZLA_WERSJA)
      log("otoczenie: wersja kontraktu %d, panel zna %d: traktuję jak brak źródła", r.oto.wersja, OTO_WERSJA);
    else if (st != StanOto::OK) log("otoczenie: odpowiedź odrzucona (stan %d)", (int)st);
    else
      log("otoczenie: pogoda %s, PM2.5 %d, CO2 %d%s", pogodaTeraz(r.oto, ep).c_str(), pm25Teraz(r.oto, ep),
          co2Teraz(r.oto, ep), r.oto.co2Milczy ? ", co2_milczy" : "");
    if (otoTeraz && r.oto.zleTs)
      log("otoczenie: odrzucone bloki z nieczytelnym ts:%s%s%s", r.oto.zleTs & 1 ? " pogoda" : "",
          r.oto.zleTs & 2 ? " powietrze" : "", r.oto.zleTs & 4 ? " sypialnia" : "");
  }

  // Odczyt AirBridge i źródła otoczenia.
  void odczyt(bool przerywalny) {
    int poprzedniKod = httpKod;
    int w = pobierz(przerywalny);
    abPrzerywanie(false);
    odczytPrzerwany = w < 0;
    if (w < 0) {
      httpKod = poprzedniKod;
      odczytUdany = r.ostatniOdczytOk;
      return;
    }
    if (tylkoStatusTeraz && w == 0) {
      // ocena slotu dziennego bez odpowiedzi `/api/status`: zwykła ocena, nie odczyt nocy z R-AW-01, więc bez E-01
      // i bez zmiany stanu ostatniego odczytu (R-KON-56)
      log("%s: brak odpowiedzi /api/status, zwykła ocena (bez E-01)", wieczorTeraz ? "slot wieczorny" : "slot dzienny");
      httpKod = poprzedniKod;
      odczytUdany = r.ostatniOdczytOk;
      pobierzOtoczenie(przerywalny);
      if (otoTeraz && zegar()) dodajCo2(r.buforSyp, dzisiaj(), minutaDoby(), co2DoBufora(r.oto, teraz()));
      return;
    }
    odczytUdany = w > 0;
    r.ostatniOdczytOk = odczytUdany;
    // (5.1.4): nieudany odczyt bez E-03 to brak odpowiedzi AirBridge (E-01 od 11:00), inaczej to, co pobierz
    // zobaczyło w sprawie ostatniej nocy
    obserwujNoc(w == 0 && httpKod != KOD_E03 ? stan_nocy::Obserwacja::BRAK_ODPOWIEDZI : obsNocy);
    r.logowanieOdrzucone = httpKod == 401;  // R-AW-34: ponawianie co 30 min także od drugiej nocy (src/plan.h)
    pobierzOtoczenie(przerywalny);
    if (otoTeraz && zegar()) dodajCo2(r.buforSyp, dzisiaj(), minutaDoby(), co2DoBufora(r.oto, teraz()));
    uint32_t przed = r.ab.bladOd;
    r.ab.bladOd = bladOdPoOdczycie(odczytUdany, r.ab.bladOd, zegar(), teraz());
    if (r.ab.bladOd != przed) zapiszStanAB();
  }

  // Pobranie po naciśnięciu: Wi-Fi wyłączone zaraz po nim, chyba że zostaje dla okna serwisowego (R-NF-03, T-NF-06);
  // poranna decyzja o pytaniu po wyłączeniu (bez sieci).
  void wykonajPobranie(const Pobranie& p, bool zostawSiec) {
    if (!p.pobierz) return;
    if (k.pierwszaKonfiguracja) {  // bez sieci domowej nie ma czego pobierać (bez E-01)
      log("pierwsza konfiguracja: bez pobierania, czekam na sieć panelu (◀ 2 s)");
      return;
    }
    odczyt(p.przerywalne);
    if (!zostawSiec) zamknijSiec();
    if (p.poranekCzynnikow) poranekCzynniki();
  }

  // Pobranie po wybudzeniu zegarem albo starcie (7.1 kroki 5–10): AirBridge i otoczenie, zaległe doby w tej samej
  // sesji Wi-Fi (krok 8), Wi-Fi wyłączone zaraz po ostatnim żądaniu (krok 9), zapis (krok 10), potem bez sieci
  // poranna decyzja o pytaniu.
  void pobranieWybudzenia(PowodWybudzenia w, const Pobranie& p) {
    if (k.pierwszaKonfiguracja && p.pobierz) {  // bez E-01, instrukcja U-02 na ekranie dnia
      log("pierwsza konfiguracja: bez pobierania, czekam na sieć panelu (◀ 2 s)");
      if (p.poranekCzynnikow) poranekCzynniki();
      return;
    }
    if (!p.pobierz) {
      if (p.poranekCzynnikow) poranekCzynniki();  // termin odpowiedzi bez Wi-Fi (R-CZY-03)
      return;
    }
    odczyt(p.przerywalne);
    WarunkiZaleglych wz;
    wz.powod = w;
    // ekran, z którym wybudzenie skończy (TEST po zegarze wraca do ekranu dnia), jak przy rysowaniu
    wz.ekran = ekranPoStarcie(w, ekran(), zegar(), (long)(int32_t)(teraz() - r.ekranOd), minutaDoby());
    wz.odczytUdany = odczytUdany;
    wz.listaOk = abListaOk;
    wz.zablokowane = r.ab.zablokowane;
    wz.plikiOk = fsOk();
    wz.wifi = a.siec.polaczona();
    wz.dataZnana = !dzisiaj().empty();
    wz.minuta = minutaDoby();
    if (!tylkoStatusTeraz && dociagacZalegle(wz)) dociagnijHistorie();
    if (prognozaTeraz) pobierzPrognozeB9();  // R-KON-52, w tej samej sesji Wi-Fi
    sprawdzenieDobowe();  // R-KONF-46: raz na dobę w tej samej sesji Wi-Fi (bez tokenu bez zapytania, R-NF-33)
    zamknijSiec();
    zapiszPrognozeB9();  // zapis po wyłączeniu Wi-Fi (7.1 krok 10)
    if (p.poranekCzynnikow) poranekCzynniki();
  }

  // Stan planu wybudzenia (src/plan.h WejsciePlanu) z RTC, NVS i plików tego wybudzenia. `obsluzone`: czas unix
  // ostatniej obsłużonej chwili terminów (0 = nieznana; późniejsza niż teraz, np. po cofnięciu zegara przez NTP, też).
  WejsciePlanu wejsciePlanu(uint32_t obsluzone) {
    WejsciePlanu w;
    std::tm t{};
    w.zegar = zegar() && a.zegar.lokalny(teraz(), t);
    if (w.zegar) w.sekundaDoby = t.tm_hour * 3600 + t.tm_min * 60 + t.tm_sec;
    std::tm o{};
    // chwila obsłużona do KOREKTA_NTP_S po teraz zostaje
    if (w.zegar && zegarZnany(obsluzone) && obsluzone <= teraz() + KOREKTA_NTP_S && a.zegar.lokalny(obsluzone, o)) {
      w.obsluzoneZnane = true;
      bool tenSamDzien = o.tm_year == t.tm_year && o.tm_yday == t.tm_yday;
      w.obsluzoneDo = tenSamDzien ? o.tm_hour * 3600 + o.tm_min * 60 + o.tm_sec : -1;
    }
    std::string dzis = dzisiaj();
    w.nocKompletna = nocKompletnaDzis(hist, dzis, wczytajStanHist().pom);  // doba pominięta też
    w.sprawdzenie11 = !dzis.empty() && nvsTekst("ab_11") == dzis;
    w.e01 = r.ab.bladOd != 0;
    std::string dOdczytu, dBledu;
    int mOdczytu = -1, m;
    if (r.ab.ma) dataIMinuta(r.ab.pobrano, dOdczytu, mOdczytu);
    dataIMinuta(r.ab.bladOd, dBledu, m);
    // to samo liczenie co ekran S5 (Przebieg::daneDnia(), src/dzien.h nocyBezDanych())
    w.nocyBezDanych = nocyBezDanych(w.e01, dzis, dOdczytu, mOdczytu, dBledu, &hist);
    w.logowanieOdrzucone = r.logowanieOdrzucone;
    w.zrodloOtoczenia = otoUzywalne();
    w.co2 = co2B17();  // wieczór z Wi-Fi tylko z ekranami CO2
    w.ekran = ekran();
    w.sekundOdNacisniecia = (long)(int32_t)(teraz() - r.ekranOd);
    w.terminPytania = terminPytaniaDzis(dzis.empty() ? nullptr : wpisZDnia(czynniki, dzis));
    std::tm d{};  // R-EKR-60: ostatnie narysowanie ekranu dnia dziś (sekunda doby), inaczej -1
    if (w.zegar && zegarZnany(r.dzienRysowanyEp) && r.dzienRysowanyEp <= teraz() && a.zegar.lokalny(r.dzienRysowanyEp, d) &&
        d.tm_year == t.tm_year && d.tm_yday == t.tm_yday)
      w.dzienRysowany = d.tm_hour * 3600 + d.tm_min * 60 + d.tm_sec;
    // (R-KON-41, R-KON-52): chwile slotu dziennego z RTC; koniec terapii z raportu prototyp nie zapisuje,
    // więc -1 (start od samego importu); konfiguracja panelu nie ma miasta (R-KONF-24), a prototyp klienta serwisu
    // pogody, więc bez prognozy godzinowej; noc bez terapii (E-10) to stany nocy 5.1.4 (poza): false
    w.importNocy = w.nocKompletna ? sekundaDzis(r.importNocyEp) : -1;
    w.slotDzienny = !k.bezSlotuDziennego;
    w.slotDziennyOd = sekundaDzis(r.slotDziennyEp);
    w.nacisniecieRano = sekundaDzis(r.nacisniecieRanoEp);
    w.koniecTerapii = -1;
    w.miasto = miastoB9();                              // współrzędne miasta w konfiguracji (R-KONF-24)
    w.prognozaGodzinowa = sekundaDzis(prognozaB9().pobrano);  // ostatnie udane pobranie prognozy dziś
    w.nocBezTerapii = !dzis.empty() && stan_nocy::bezTerapii(fazaNocy(), dzis);  // E-10 z 5.1.4 (R-DANE-18)
    w.wieczorOd = slot_wieczorny::oknoZHistorii(hist, dzis);  // R-KON-09, R-KON-17
    w.niedziela = dataOk(dzis) && dzienTygodnia(dzis) == 0;   // slot tygodniowy (R-KON-08)
    // szybszy wynik nocy rano: gęste okno (R-DANE-61) i powtórka przy przetwarzaniu (R-DANE-62)
    w.zwyklyKoniec = zwyklyKoniecTerapii(hist, dzis);
    w.przetwarzaOd = sekundaDzis(r.przetwarzaEp);
    w.powtorekDzis = r.powtorkiDoba != 0 && r.powtorkiDoba == dobaPamieci() ? r.powtorki : 0;
    w.drugiEkranPortretu = dzis.empty() ? -1 : minutaDrugiegoEkranu(dzis);  // R-POR-04
    return w;
  }

  // po zaniku zasilania RTC nie zna chwili importu nocy kompletnej dziś;
  // zastępnik zapisany raz, przed pobraniem tego wybudzenia: chwila ostatniego udanego odczytu AirBridge z NVS
  // (StanAB::pobrano sprzed zaniku, nie wcześniejsza niż import tej nocy). Start slotu = pobrano + 60 min, a gdy
  // minął, od razu (start) albo przy najbliższym wybudzeniu (R-KON-41, „panel uśpiony”); późniejsze pobrania (np. po
  // naciśnięciu) już go nie przesuwają.
  void zastepnikImportu() {
    if (sekundaDzis(r.importNocyEp) >= 0) return;
    std::string dzis = dzisiaj();
    // (R1 karty): chwila importu z rekordu nocy (pole `import`); zastępnik z NVS tylko przy pustym polu
    const Noc* nd = dzis.empty() ? nullptr : nocZDnia(hist, dzis);
    if (nd && nd->import && sekundaDzis(nd->import) >= 0 && nocKompletnaDzis(hist, dzis, wczytajStanHist().pom)) {
      r.importNocyEp = nd->import;
      log("slot dzienny: chwila importu z rekordu nocy %s", godzina(nd->import).c_str());
      return;
    }
    if (!r.ab.ma || sekundaDzis(r.ab.pobrano) < 0) return;
    if (dzis.empty() || !nocKompletnaDzis(hist, dzis, wczytajStanHist().pom)) return;
    r.importNocyEp = r.ab.pobrano;
    log("slot dzienny: chwila importu nieznana, zastępnik z odczytu AirBridge %s", godzina(r.ab.pobrano).c_str());
  }

  // Sekunda doby dziś (czas lokalny) chwili `ep` albo -1: zegar nieznany, chwila nieznana, z innego dnia albo później
  // niż teraz (cofnięty zegar; chwile z tego samego wybudzenia sprzed NTP zapisuje dyrygent dopiero po pobraniu).
  int sekundaDzis(uint32_t ep) {
    std::tm t{}, e{};
    if (!zegar() || !zegarZnany(ep) || ep > teraz() || !a.zegar.lokalny(teraz(), t) || !a.zegar.lokalny(ep, e))
      return -1;
    if (e.tm_year != t.tm_year || e.tm_yday != t.tm_yday) return -1;
    return e.tm_hour * 3600 + e.tm_min * 60 + e.tm_sec;
  }

  // ---------- okno serwisowe ----------
  void startOta() {
    if (otaAktywne || !a.siec.polaczona()) return;
    otaAktywne = a.siec.otworzOknoSerwisowe();
  }
  // Okno serwisowe na ekranie testu; gdy się nie otworzy (np. bez OTA_PASS), Wi-Fi wyłączone przed rysowaniem
  // i czuwaniem (T-NF-06): sieć zostaje tylko dla otwartego okna.
  void otworzOknoAlboZamknijSiec() {
    startOta();
    if (!otaAktywne && a.siec.polaczona()) zamknijSiec();
  }

  // ---------- przyciski ----------
  Przycisk wcisniety() { return wcisnietyPrzycisk(a.przyciski.wcisniete(), zaciete); }

  void czekajNaPuszczenie() {
    uint32_t tp = ms();
    while (wcisniety() != Przycisk::BRAK && ms() - tp < ZACIECIE_MS) czekaj(10);
    Przycisk p = wcisniety();
    uint8_t z = zacietePoCzekaniu(p, zaciete);
    if (z != zaciete) log("przycisk %d zacięty: ignoruję do puszczenia", (int)p);
    zaciete = z;
  }

  // Czeka, aż przycisk zostanie puszczony albo minie próg przytrzymania (licząc od `od` w ms).
  bool przytrzymany(Przycisk p, uint32_t od) {
    uint8_t bit = (uint8_t)(1u << (int)p);
    for (;;) {
      Przytrzymanie s = przytrzymanie(od, ms(), (a.przyciski.wcisniete() & bit) != 0);
      if (s != Przytrzymanie::TRWA) return s == Przytrzymanie::DLUGO;
      czekaj(5);
    }
  }

  void otworzPytanie() {
    std::string dzis = dzisiaj();
    const WpisCzynnikow* w = dzis.empty() ? nullptr : wpisZDnia(czynniki, dzis);
    bool zLinii = ekran() == Ekran::DZIEN && zaproszenieAktywne(w, dzis, minutaDoby());
    std::string data;
    if (zLinii) {
      data = dzis;
    } else {
      const Noc* ost = ostatniaNoc(hist, dzis.empty() ? "9999-99-99" : dzis);  // R-CZY-08
      if (ost) data = ost->data;
    }
    r.pyt = StanPytania();
    kopiuj(r.pyt.data, sizeof r.pyt.data, data);
    r.pyt.kursor = POZYCJA_NIC;  // R-CZY-18
    r.pyt.ruszone = false;
    const WpisCzynnikow* stary = wpisZDnia(czynniki, data);
    r.pyt.zaznaczone = stary && stary->stan == 'O' ? stary->czynniki : 0;
    r.pyt.termin = (int16_t)(zLinii ? w->termin : -1);
    r.pyt.innaNoc = zLinii && w->warstwa == 'O';
  }

  // M-25 (E-05, R-KONF-41): krzywa napięcie → procent, wygładzenie w RTC; brak napięcia: ostatni procent z RTC przez
  // jedno wybudzenie, potem brak M-25; poniżej 3,0 V brak odczytu
  void procentBaterii25() {
    StanProcentuBaterii sp;
    sp.proc = r.bateriaProc;
    sp.surowy = r.bateriaSurowy;
    int surowy = std::isnan(bateria) ? -1 : procentBaterii((int)std::lround(bateria * 1000));
    bateriaZOdczytu = surowy >= 0;
    procentBat = wygladzProcent(sp, surowy);
    r.bateriaProc = sp.proc;
    r.bateriaSurowy = sp.surowy;
  }

  // Naciśnięcie. Ponowienie pobrania (↻ na S5) idzie tu, przed zmianą ekranu, jak w prototypie;
  // pobranie na ekranie testu zostaje dla wołającego (PoNacisnieciu::pobranie).
  PoNacisnieciu obsluz(Przycisk p, bool dlugo, bool budzacy) {
    static const char* const N[] = {"-", "◀", "↻", "▶"};
    ostatniPrzycisk = std::string(N[(int)p]) + (dlugo ? " 2 s" : "");
    DecyzjaNocy dn = nocTeraz(false);
    std::string dzis = dzisiaj();
    KontekstNaw kn = kontekstNawigacji(ekran(), dn, s5Teraz(), s10Teraz(), zaproszenieTeraz(),
                                       ostatniaNoc(hist, dzis.empty() ? "9999-99-99" : dzis) != nullptr);
    Ekran przed = ekran();
    if (!dzis.empty() && silnikDzis(dzis)) {  // odmiany ekranu dnia i „Otwarte sprawy”
      const sprawy::SprawaL* e = sprawaB5(b5.kluczEkranu);
      kn.pytanieTN = przed == Ekran::DZIEN && !kn.nocny && b5.pytanie.jest;
      kn.wzorzec = przed == Ekran::DZIEN && !kn.nocny && !kn.pytanieTN && e;
      kn.problem = e && e->ton == sprawy::Ton::PROBLEM;
      kn.omowienieZablokowane = e && wariantOmowieniaB11(*e, dzis) == 5;  // R-LEK-26
      kn.spraw = przed == Ekran::SPRAWY ? kartySprawB5(dzis) : 0;
      // karta z NVS przycięta do liczby kart (liczba mogła spaść przy otwartym ekranie)
      kn.sprawa = przed == Ekran::SPRAWY ? przytnijKarte(b5.karta, kn.spraw) : b5.karta;
      if (przed == Ekran::SPRAWY) b5.karta = kn.sprawa;
      kn.cofniecie = cofniecieB5();
    }
    kn.sprawyDostepne = sprawyWCiaguB5(dzis);  // R-KOK-20: liczba spraw w chwili naciśnięcia
    kn.pytanieRuszone = r.pyt.ruszone;
    kn.s5Bateria = kn.s5 && s5BateriiTeraz();  // ↻ na S5 baterii mierzy od nowa, bez pobrania
    kn.profilPubliczny = profilB18();  // (R-KOK-22)
    // ◀ 2 s na S-K-31 (R-MAS-01) i S-K-35 (R-MAS-18): element zapisany przy rysowaniu ekranu dnia (`b16_sk31`)
    // S-K-31 narysowany zamiast ekranu L-09 albo zdarzenia E (etap 0): przyciski S-K-31, nie ekranu L
    kn.sk31 = przed == Ekran::DZIEN && !kn.nocny && !kn.pytanieTN && a.nvs.liczbaU("cpap", "b16_sk31", 0) > 0;
    if (kn.sk31) kn.wzorzec = kn.problem = false;
    kn.zrodlo = przed == Ekran::RAPORT ? rap.z : przed == Ekran::SIEC_PANELU ? ap.z : Ekran::DZIEN;
    kn.bezKonfiguracji = k.pierwszaKonfiguracja;
    if (przed == Ekran::AKTUALIZACJA) {  // U-07: ▶ tylko w stanach z pytaniem albo ponowieniem (R-KONF-44)
      aktualizacja::StanU07 su = daneU07().stan;
      kn.aktualizacja = su == aktualizacja::StanU07::NOWSZA ? 1
                        : (su == aktualizacja::StanU07::PO_NIEUDANEJ || su == aktualizacja::StanU07::SPRAWDZENIE_NIEUDANE) ? 2
                                                                                                                 : 0;
    }
    if (przed == Ekran::SPRZET_ZAPISANO) kn.cofniecie = cofniecieWymianyB16();
    // R-KON-41: naciśnięcie na ekranie dnia po imporcie nocy zaczyna slot dzienny od tej chwili
    // R-POR-05: na Portrecie przed upływem 30 min naciśnięcie nie zaczyna slotu dziennego (↻ z kokpitu wraca do
    // Portretu); slot zaczyna naciśnięcie na drugim ekranie albo, bez niego, na Portrecie po 30 min
    // R-POR-24 „rano”: ▶ otwierające stronę Portretu (ekran problemu albo drugi ekran poranka) też nie zaczyna slotu;
    // naciśnięcia na stronie Portretu i w kokpicie slotu nie zaczynają (startSlotuPrzyciskiem() tylko przy ekranie dnia),
    // więc slot zaczyna wtedy start z planu albo naciśnięcie na ekranie dnia po powrocie (R-POR-05)
    const bool otwieraPortret = przed == Ekran::DZIEN && r.portretStrona && !dlugo && p == Przycisk::PRAWY &&
                                sekundaDzis(r.slotDziennyEp) < 0;
    if (startSlotuPrzyciskiem(wejsciePlanu(0), przed) && !portretWToku(dzis) && !otwieraPortret) {
      r.nacisniecieRanoEp = r.slotDziennyEp = teraz();
      log("slot dzienny: start naciśnięciem (R-KON-41)");
    }
    Przejscie pr;
    if (!przejsciePortretu(przed, p, dlugo, kn, pr)) pr = nacisniecie(przed, p, dlugo, kn);
    // sieć panelu w nocy (slot nocny albo aparat pracuje, `rop` = 1) bez buzzera
    bool cicho = pr.akcja == Akcja::SIEC_PANELU_START && ((zegar() && slotNocny(minutaDoby())) || r.ab.rop == 1);
    // adapter z piskiem przy naciśnięciu (R-PRZ-20) już zapiszczał raz, także za przycisk budzący (pierwsza czynność
    // programu): tu tylko drugi pisk przytrzymania; naciśnięcie z kolejki bez pisku tutaj
    int piskow = dlugo ? 2 : 1;
    if (a.przyciski.piskPrzyNacisnieciu()) piskow--;
    if (!cicho && !zKolejkiTeraz && piskow > 0) a.dzwiek.pisk(piskow);
    PoNacisnieciu w;
    w.pobranie = pobraniePoNacisnieciu(pr, przed, budzacy);
    switch (pr.akcja) {
      case Akcja::PONOW_POBRANIE:
        wykonajPobranie(w.pobranie, otaAktywne);  // przerywalne kolejnym naciśnięciem
        w.pobranie = Pobranie();
        break;
      case Akcja::POMIAR_BATERII:  // nowy pomiar (bez Wi-Fi), silnik od nowa, ekran dnia z nowym wyborem
        if (!budzacy) {  // przycisk budzący: pomiar z tego wybudzenia jest świeży (przed ekranem i Wi-Fi)
          bateria = a.czujniki.czytaj().bateriaV;
          procentBaterii25();
        }
        b5.dzis.clear();
        log("bateria: ↻ na S5, pomiar %s", procentBat >= 0 ? (std::to_string(procentBat) + "%").c_str() : "brak");
        break;
      case Akcja::OTWORZ_PYTANIE: otworzPytanie(); break;
      case Akcja::PYT_ZAZNACZ:
        r.pyt.zaznaczone = przelacz(r.pyt.zaznaczone, r.pyt.kursor);
        r.pyt.ruszone = true;  // coś zmieniono albo wybrano, ↻ 2 s zapisze
        break;
      case Akcja::PYT_WSTECZ: r.pyt.kursor = (uint8_t)((r.pyt.kursor + 7) % 8); break;
      case Akcja::PYT_DALEJ: r.pyt.kursor = (uint8_t)((r.pyt.kursor + 1) % 8); break;
      case Akcja::PYT_ZAPISZ:  // R-CZY-10, R-CZY-14
        if (r.pyt.data[0]) {
          zapiszOdpowiedz(czynniki, r.pyt.data, r.pyt.zaznaczone, r.pyt.termin >= 0);
          zapiszCzynniki();
        }
        break;
      case Akcja::PYT_POMIN:
        if (r.pyt.data[0]) {
          pominPytanie(czynniki, r.pyt.data);
          zapiszCzynniki();
        }
        break;
      case Akcja::RAPORT_STARSZE:  // R-LEK-33
        if (rap.serwer) {
          rap.sesja.pokazStarsze();
          rap.stan = "Starsze raporty widoczne do " + godzina(rap.koniecEp) + ".";
        }
        break;
      case Akcja::SPRZET_WYMIENIONE: {  // R-MAS-01, R-MAS-18: ◀ 2 s na S-K-31 albo S-K-35 (element z ekranu dnia)
        int el = (int)a.nvs.liczbaU("cpap", "b16_sk31", 0) - 1;
        // ekran z cofnięciem 30 s; stan sprzed zapisu w NVS `sp_cofw` (`{element};{data}`)
        if (el >= 0 && el < EL_N) a.nvs.ustawTekst("cpap", "sp_cofw", std::to_string(el) + ";" + ustawieniaB9().sprzet[el].wymieniono);
        wymianaB16(el);
        b5.dzis.clear();
        break;
      }
      case Akcja::SPRZET_COFNIJ: cofnijWymianeB16(); break;  // R-MAS-01
      case Akcja::SIEC_PANELU_START:  // ◀ 2 s na U-01, U-05, pierwszym uruchomieniu; ↻ 2 s na U-02
        log("sieć panelu: uruchomiona przyciskiem%s", cicho ? " (noc, bez buzzera)" : "");
        otworzSiecPaneluP07(przed == Ekran::SIEC_PANELU ? ap.z : przed, false);
        break;
      case Akcja::SIEC_PANELU_ANULUJ:  // ◀ na U-02 (R-KONF-29)
        log("sieć panelu: anulowana %s", p == Przycisk::LEWY ? "◀" : "↻");  // ◀ tam, skąd uruchomiono, ↻ dom
        ap.stan.clear();
        zamknijSiecPaneluP07();
        break;
      case Akcja::AKT_SPRAWDZ: akcjaSprawdz(); break;   // ↻ 2 s na U-01 (R-KONF-43)
      case Akcja::AKT_INSTALUJ: akcjaInstaluj(); break;  // ▶ na U-07 (R-KONF-44)
      case Akcja::AKT_PONOW:                             // ▶ spróbuj ponownie: ta sama czynność
        if (a.nvs.liczbaU(aktualizacja::PRZESTRZEN, "u07_op", 0) == 1) akcjaInstaluj();
        else akcjaSprawdz();
        break;
      default: akcjaB5(pr.akcja, pr.ekran, dzis); break;
    }
    if (przed == Ekran::RAPORT && pr.ekran != Ekran::RAPORT) zamknijRaportB13();  // R-LEK-11
    if (pr.ekran == Ekran::RAPORT && przed != Ekran::RAPORT) otworzRaportB13(dzis, przed);  // R-LEK-02
    if (przed == Ekran::FORMULARZ && pr.ekran != Ekran::FORMULARZ) zamknijFormularzP07();  // R-KONF-16
    if (przed == Ekran::SIEC_PANELU && pr.ekran != Ekran::SIEC_PANELU) zamknijSiecPaneluP07();  //
    if (pr.ekran == Ekran::FORMULARZ && przed != Ekran::FORMULARZ) {  // R-KONF-12
      frm.stan.clear();
      otworzFormularzP07();
    }
    w.zmiana = pr.akcja != Akcja::NIC || pr.ekran != ekran();
    ustawEkran(pr.ekran);
    r.ekranOd = teraz();
    return w;
  }

  // ---------- szybszy wynik nocy rano (R-DANE-62, R-PRZ-22) ----------
  // Wybudzenie z powodem powtórki: licznik doby +1 i chwila przetwarzania skasowana (pobranie tego wybudzenia ustawi ją
  // od nowa, gdy moduł nadal przetwarza), więc powtórek nie ma więcej niż PLAN_POWTORKI_MAKS, nawet bez odczytu.
  void policzPowtorke() {
    int32_t doba = dobaPamieci();
    r.powtorki = (uint8_t)(r.powtorkiDoba == doba && doba != 0 ? r.powtorki + 1 : 1);
    r.powtorkiDoba = doba;
    r.przetwarzaEp = 0;
    log("powtórka przy przetwarzaniu %d z %d (R-DANE-62)", (int)r.powtorki, PLAN_POWTORKI_MAKS);
  }

  // Czy przycisk budzący ma rano dodatkowo pobrać noc (src/plan.h pobranieRanoPrzyciskiem()): nie, gdy akcja przycisku
  // sama pobiera albo już pobrała (↻ na S5 i S10, ekran testu), trwa sieć panelu, serwer raportu albo formularza, okno serwisowe albo
  // pierwsza konfiguracja.
  bool pobranieRanoTeraz(const Pobranie& akcji) {
    if (akcji.pobierz || abPobierano || k.pierwszaKonfiguracja || otaAktywne || ap.aktywna || rap.serwer || frm.serwer) return false;
    // tylko na ekranie dnia i stronach kokpitu: tam kolejka naciśnięć z czasu pobierania jest wykonywana,
    // a krótkie naciśnięcie nic nie zapisuje (także „Otwarte sprawy” i „Co znaczą te liczby” bez numeru, R-KOK-19);
    // pytania, potwierdzenia, U-01 i inne U-xx bez pobrania (R-PRZ-22)
    if (ekran() != Ekran::DZIEN && !(stronaKokpitu(ekran()) && ekran() != Ekran::USTAWIENIA)) return false;
    // pytanie tak/nie na ekranie dnia (5.12.8): krótkie naciśnięcie zapisuje odpowiedź, więc przytrzymanie wykryte
    // z opóźnieniem po żądaniu HTTP mogłoby zapisać „tak” albo „nie”; tam bez pobrania
    if (ekran() == Ekran::DZIEN && b5.pytanie.jest) return false;
    long od = -1;
    if (zegar() && zegarZnany(r.pobranieRanoEp) && r.pobranieRanoEp <= teraz()) od = (long)(teraz() - r.pobranieRanoEp);
    return pobranieRanoPrzyciskiem(wejsciePlanu(0), od);
  }

  // Pobranie przerywalne po wybudzeniu przyciskiem rano bez nocy (R-PRZ-22), z poranną decyzją o pytaniu po
  // wyłączeniu Wi-Fi (jak ↻ na S5). Naciśnięcie w trakcie przerywa je i zostaje w kolejce do czuwania. Nowa noc i ekran dnia na panelu: ekran dnia od nowa;
  // w kokpicie nowa noc jest widoczna przy następnym rysowaniu.
  void pobierzRanoPoPrzycisku() {
    pobranieRano = false;
    std::string dzis = dzisiaj();
    bool byla = nocKompletnaDzis(hist, dzis, wczytajStanHist().pom);
    log("rano bez nocy: pobranie po przycisku, przerywalne (R-PRZ-22)");
    Pobranie p;
    p.pobierz = p.przerywalne = true;
    p.poranekCzynnikow = true;  // decyzja o pytaniu od razu, jak ↻ na S5 i S10
    abZostawKolejke = true;
    odczytPrzerwany = false;
    wykonajPobranie(p, false);
    abZostawKolejke = false;
    // limit 2 min od końca pobrania, które nie zostało przerwane naciśnięciem
    if (!odczytPrzerwany && zegar()) r.pobranieRanoEp = teraz();
    bool jest = nocKompletnaDzis(hist, dzis, wczytajStanHist().pom);
    if (!byla && jest && ekran() == Ekran::DZIEN) {
      log("rano: nowa noc po przycisku, ekran dnia od nowa");
      pokaz();
    }
  }

  // ---------- sen ----------
  // Tuż przed każdym snem: wiersz pamięci, licznik startów od zera, sen (T-WYM-82). Wersja
  // niesprawdzona nie zasypia (7.8): zamiast snu powrót do poprzedniej.
  void przedSnem(const PlanSnu& plan) {
    PamiecWybudzenia pm = pamiecTeraz();
    zapamietajPamiec(pm);
    a.port.wiersz(wierszPamieci(pm));
    if (!wolnoZasnac(a.uruchomienie.stanWersji())) {
      log("wersja niesprawdzona: powrót do poprzedniej zamiast snu (T-WYM-41)");
      a.uruchomienie.wrocDoPoprzedniej();
      return;
    }
    zerujPrzedSnem(a.uruchomienie.licznik(), wersjaId);
    odnow();
    a.sen.spij(plan);
  }

  // Chwila, do której to wybudzenie wykonało terminy planu: zegar ze znanym czasem → chwila
  // powodyTeraz() na początku; przycisk → bez zmiany (przycisk nie wykonuje terminów), chyba że nieznana; start
  // i zegar bez czasu (pobranie z Wi-Fi) → początek wybudzenia według zegara po NTP. Bez zegara 0.
  uint32_t chwilaObsluzona() {
    if (!zegar()) return 0;
    if (obsluzonePrzyStarcie) return obsluzonePrzyStarcie;
    if (wybudzenieZ == PowodWybudzenia::PRZYCISK && zegarZnany(r.obsluzoneDo) && r.obsluzoneDo <= teraz())
      return r.obsluzoneDo;
    return teraz() - ms() / 1000;
  }

  // Sen do najbliższego terminu z planu (T-WYM-28, R-NF-02), budzą przyciski niewciśnięte (T-WYM-29). Termin
  // z powodem do wykonania, który wypadł w trakcie tego wybudzenia: sen ZALEGLY_TERMIN_S.
  void spij() {
    zamknijSiec();
    zapiszWpisBaterii();
    r.obsluzoneDo = chwilaObsluzona();
    WejsciePlanu wp = wejsciePlanu(r.obsluzoneDo);
    Termin t = nastepnyTermin(wp);
    if (powodyZalegle(wp)) log("termin w trakcie wybudzenia: %s", nazwyPowodow(t.powody).c_str());
    wcisniety();  // zaciete bez puszczonych
    r.zaciete = zaciete;  // zacięty ignorowany także w kolejnych wybudzeniach
    PlanSnu plan = planSnuZTerminu(t, a.przyciski.wcisniete());
    log("sen na %lu s, następny termin: %s", (unsigned long)plan.sekund, nazwyPowodow(t.powody).c_str());
    przedSnem(plan);
  }

  // ---------- samosprawdzenie nowej wersji (T-WYM-41, 7.8) ----------
  // Zwraca false, gdy wersja wraca do poprzedniej (dyrygent kończy wybudzenie bez snu).
  bool samosprawdzenie(bool narysowano) {
    StanWersji sw = a.uruchomienie.stanWersji();
    if (!samosprawdzenieWymagane(sw)) return true;
    WynikSamosprawdzenia wy;
    wy.plikiMontuja = a.pliki.zamontowane() && !staryUklad;  // R7: stara tablica = powrót do poprzedniej wersji
    wy.migracjeOk = migracjeOk;  // T-WYM-43: migracje na nowe ścieżki z wczytajPliki()
    wy.ekranRysuje = narysowano;
    if (!wifiProbowano) {  // np. wybudzenie przyciskiem: jedno połączenie tylko do sprawdzenia (bez drugiej próby)
      wifiProbowano = true;
      bool przerwane = false;
      uint32_t t0 = ms();
      wifiByloPolaczone = a.siec.polacz(LIMIT_WIFI_MS, [this]() {
        odnow();
        return false;
      }, przerwane);
      wifiLacznieMs += ms() - t0;
      if (!otaAktywne) a.siec.wylacz();  // tylko do sprawdzenia: bez radia w czuwaniu (T-NF-06)
    }
    wy.wifiLaczy = wifiByloPolaczone;
    PoSamosprawdzeniu d = decyzjaSamosprawdzenia(sw, wy);
    log("samosprawdzenie: pliki %d, migracje %d, ekran %d, Wi-Fi %d → %s", wy.plikiMontuja, wy.migracjeOk,
        wy.ekranRysuje, wy.wifiLaczy, d == PoSamosprawdzeniu::OZNACZ_DOBRA ? "wersja dobra" : "powrót do poprzedniej");
    if (d == PoSamosprawdzeniu::OZNACZ_DOBRA) {
      a.uruchomienie.oznaczWersjeDobra();
      potwierdzInstalacje();  // aktualizacja z wydania: wynik zapisany dopiero przy wersji dobrej
      poOznaczeniu();  // T-WYM-43: migracje w miejscu i NVS dopiero po oznaczeniu
      return true;
    }
    if (d == PoSamosprawdzeniu::WROC_DO_POPRZEDNIEJ) {
      // pliki z migracji na nowe ścieżki usunięte: poprzednia wersja dopisuje do plików prototypu, a następna próba
      // nowej przeniesie je od nowa (pliki prototypu nietknięte, T-WYM-43)
      if (fsOk()) mag.wycofajMigracje();
      a.siec.wylacz();
      a.uruchomienie.wrocDoPoprzedniej();
      return false;
    }
    return true;
  }

  // ---------- zwykłe wybudzenie (od kroku 2 plan wybudzenia src/plan.h) ----------
  void zwykleWybudzenie(PowodWybudzenia w) {
    // przyciski zacięte przed snem i nadal wciśnięte: ignorowane do puszczenia (T-WYM-29)
    zaciete = (uint8_t)(r.zaciete & a.przyciski.wcisniete());
    Przycisk budzacy = Przycisk::BRAK;
    bool budzacyDlugo = false;
    bool zmiana = true;  // naciśnięcie budzące coś zmieniło (inaczej w nocy bez przerysowania)
    if (w == PowodWybudzenia::PRZYCISK) {
      // przytrzymanie liczone od startu programu (ms = 0), a przycisk był wciśnięty już przed startem (T-WYM-21)
      budzacy = przyciskBudzacy(a.uruchomienie.przyciskiBudzace());
      skrotBudzenia = przytrzymanieLubSkrot(budzacy, 0, budzacyDlugo);
      if (skrotBudzenia == 1) budzacy = Przycisk::PRAWY;
    }
    r.wybudzen++;

    sprawdzWersjeNvs();  // T-WYM-54 dla NVS: przed pierwszym zapisem do NVS w tym wybudzeniu
    aktualizacjaPoStarcie(w);  // wynik instalacji z wydania (R-KONF-44, R-NF-35)
    if (r.ab.magia != MAGIA_AB) {  // po zaniku zasilania: odtwórz z NVS
      if (a.nvs.bajty("noc", "ab", &r.ab, sizeof r.ab) != sizeof r.ab || r.ab.magia != MAGIA_AB) {
        r.ab = StanAB();
        r.ab.rop = -1;
        r.ab.zalegle = -1;
        r.ab.magia = MAGIA_AB;
      }
    }
    // RTC po starcie innym niż wybudzenie z uśpienia nie ma magii; inny adres źródła: bez starych danych
    if (r.oto.magia != OTO_MAGIA || r.oto.adres != otoAdres()) wyczyscOtoczenie(r.oto, otoSkonfigurowane(), otoAdres());
    if (otoSkonfigurowane() && !otoUzywalne() && r.oto.stan != (int8_t)StanOto::POZA_SIECIA) {
      r.oto.stan = (int8_t)StanOto::POZA_SIECIA;  // R-NF-27: bez żądania i tokenu (adres bez tokenu w logu)
      log("otoczenie: adres poza siecią domową, bez zapytań (R-NF-27)");
    }
    uint32_t zNvs = a.nvs.liczbaU("cpap", "ziarno", 0);  // ziarno grupy kontrolnej (R-CZY-02): raz na panel
    ziarno = ziarnoPanelu(zNvs, zNvs ? 0 : a.siec.losowa());
    if (ziarno != zNvs) a.nvs.ustawLiczbeU("cpap", "ziarno", ziarno);
    stanNocy.wieczor = a.nvs.tekst("cpap", "noc_w", "");
    stanNocy.s9 = a.nvs.flaga("cpap", "noc_s9", false);
    stanE08.od = a.nvs.liczbaU("cpap", "e08_od", 0);
    stanE08.dzien = a.nvs.tekst("cpap", "e08_d", "");
    stanE08.koniec = a.nvs.flaga("cpap", "e08_k", false);

    OdczytCzujnikow cz = a.czujniki.czytaj();  // bateria przed ekranem i Wi-Fi
    temp = cz.temp;
    wilg = cz.wilg;
    bateria = cz.bateriaV;
    procentBaterii25();

    wczytajPliki(w);
    // T-WYM-87: okno przywracania przed jakimkolwiek Wi-Fi, przy każdym starcie zasilania (i po pierwszym
    // formatowaniu), dopóki przywracanie czeka (U2 wariant A)
    // przy starcie tylko po prawdziwym starcie zasilania (startZasilania())
    if ((sformatowano || (w == PowodWybudzenia::START && startZasilania())) && przywracanieCzeka())
      oknoPrzywracania();
    sypialniaTeraz();
    zastepnikImportu();
    // 7.1 krok 4: powody tego wybudzenia z planu (R-NF-02); po zegarze bez powodu sieciowego bez Wi-Fi (R-NF-03)
    // terminy od ostatniej obsłużonej chwili (RTC), nie tylko z ostatniej minuty
    wybudzenieZ = w;
    Powody powody = w == PowodWybudzenia::ZEGAR ? powodyTeraz(wejsciePlanu(r.obsluzoneDo)) : 0;
    if (w == PowodWybudzenia::ZEGAR && zegar()) obsluzonePrzyStarcie = teraz();
    if (powody & POWOD_POWTORKA) policzPowtorke();  // R-DANE-62: licznik doby przed pobraniem
    // termin okna porannego albo gęstego i tak odczyta status: powtórka czekająca na tę chwilę przepada bez licznika
    // (bez tego nieudane połączenie zostawiłoby zaległą powtórkę na sen 1 s)
    if (powody & (POWOD_OKNO_PORANNE | POWOD_OKNO_GESTE)) r.przetwarzaEp = 0;
    powodBaterii = powodWpisuBaterii(w, powody);
    // tylko przerysowanie R-EKR-60 (ewentualnie z zapisem czujników), bez pobrania: tryb i linia z RTC
    // ocena slotu dziennego (R-KON-41, R-KON-42) przerysowuje ekran dnia tak samo
    przerysowanie = w == PowodWybudzenia::ZEGAR && przerysowanieDnia(powody);
    tylkoStatusTeraz = w == PowodWybudzenia::ZEGAR && tylkoStatus(powody);
    wieczorTeraz = tylkoStatusTeraz && (powody & POWOD_SLOT_WIECZORNY) && !(powody & (POWOD_SLOT_DZIENNY_START | POWOD_SLOT_DZIENNY_GODZINA | POWOD_PROGNOZA_GODZINOWA));
    prognozaTeraz = w == PowodWybudzenia::ZEGAR && (powody & POWOD_PROGNOZA_GODZINOWA);  // R-KON-52
    stopkaZPamieci = w == PowodWybudzenia::ZEGAR &&
                     (((powody & POWOD_SLOT_WIECZORNY_BEZ_SIECI) &&
                       (powody & (Powody)~(POWOD_SLOT_WIECZORNY_BEZ_SIECI | POWOD_CZUJNIKI)) == 0) ||
                      ((powody & POWOD_SLOT_DZIENNY_GODZINA) &&
                       (powody & (Powody)~(POWOD_SLOT_DZIENNY_GODZINA | POWOD_CZUJNIKI)) == 0));
    log("cpap-eink %s, wybudzenie %s, licznik %lu", a.uruchomienie.wersja(), powodBaterii.c_str(),
        (unsigned long)r.wybudzen);
    // skąd sieć i AirBridge (src/sekrety.h), bez haseł (R-KONF-38)
    if (!k.opisSekretow.empty()) log("konfiguracja: %s", k.opisSekretow.c_str());
    odnow();
    if (wybudzeniePomiarowe(w, powody) && !samosprawdzenieWymagane(a.uruchomienie.stanWersji())) {
      // T-WYM-31: odczyt czujnika już w buforze nocy (sypialniaTeraz()); bez Wi-Fi, bez rysowania, od razu sen
      log("wybudzenie pomiarowe: bez Wi-Fi i bez rysowania");
      powod = "zegar";
      czuwanie(w);  // tylko przycisk wciśnięty w tej chwili (R-PRZ-09) i komendy portu serwisowego
      spij();
      return;
    }
    a.przyciski.dioda(true);

    if (w == PowodWybudzenia::PRZYCISK) {
      powod = "przycisk";
      odczytUdany = r.ostatniOdczytOk;  // tylko zmiana widoku, dane bez zmian
      PoNacisnieciu pn;
      if (skrotBudzenia != 2) pn = obsluz(budzacy, budzacyDlugo, true);
      if (koniecPoRestarcie) return;  // instalacja z U-07 zakończona restartem
      zmiana = pn.zmiana;
      wykonajPobranie(pn.pobranie, otworzycOknoSerwisowe(ekran()));  // ekran testu: świeży stan sieci, Wi-Fi dla okna
      czekajNaPuszczenie();
      // R-PRZ-22: rano bez nocy przycisk budzący dodatkowo pobiera noc, po narysowaniu wyniku akcji
      pobranieRano = skrotBudzenia != 2 && pobranieRanoTeraz(pn.pobranie);
    } else {
      powod = w == PowodWybudzenia::ZEGAR ? "zegar" : "start";
      Pobranie p = w == PowodWybudzenia::ZEGAR ? pobranieZegarem(powody) : pobranieNaStarcie(w);
      if (!p.pobierz) log("plan: bez powodu sieciowego, bez Wi-Fi (R-NF-03)");
      pobranieWybudzenia(w, p);
      // R-KON-41: start slotu dziennego zapisany po pobraniu, czyli po NTP; START tylko
      // gdy slot nie trwa (także slot bez nocy sprzed importu), start bez sieci (E-01) tylko raz na dobę
      // start bez sieci (E-01) tylko, gdy po pobraniu nadal nie ma nocy (noc pobrana w tym
      // wybudzeniu daje start 60 min po imporcie)
      WejsciePlanu po = wejsciePlanu(0);
      bool start = (powody & POWOD_SLOT_DZIENNY_START) ||
                   ((powody & POWOD_SLOT_DZIENNY_BEZ_SIECI) && sekundaDzis(r.slotDziennyEp) < 0 && !po.nocKompletna);
      // start zasilania po zaniku, gdy chwila startu slotu już minęła (zastępnik importu): slot od teraz
      int s0 = poczatekSlotuDziennego(po);
      if (w == PowodWybudzenia::START && po.slotDziennyOd < 0 && s0 >= 0 && s0 <= po.sekundaDoby) start = true;
      // (R-POR-04): start slotu w poranku z Portretem przed drugim ekranem albo razem z nim czeka, aż drugi
      // ekran stanie (start zaraz po jego narysowaniu, rysujDzien())
      if (start && zegar() && portretOdDzis(dzisiaj()) != 0 && !r.portretDrugi) {
        start = false;
        r.slotOdlozony = 1;
        log("slot dzienny: start po drugim ekranie poranka (Portret nocy, R-POR-04)");
      }
      if ((w == PowodWybudzenia::ZEGAR || w == PowodWybudzenia::START) && start && zegar()) {
        r.slotDziennyEp = teraz();
        if (w == PowodWybudzenia::START) log("slot dzienny: start przy starcie zasilania (chwila startu minęła)");
      }
    }
    odnow();
    // R-PRZ-02, R-PRZ-18: powrót po 5 min; ekran testu po zegarze wraca do ekranu dnia (okno nie otwiera się samo)
    ustawEkran(ekranPoStarcie(w, ekran(), zegar(), (long)(int32_t)(teraz() - r.ekranOd), minutaDoby()));
    if (ekranPoInstalacji()) {  // po ręcznej instalacji: wynik na U-07 (R-KONF-44, R-NF-35)
      ustawEkran(Ekran::AKTUALIZACJA);
      r.ekranOd = teraz();
    }
    // pierwszy start (ADR-0011 pkt 3): panel bez żadnej sieci (pusty NVS `sekrety`
    // i secrets.h bez WIFI_*) po starcie zasilania sam uruchamia sieć panelu; po 15 min bez zapisu zostaje instrukcja.
    // Wybudzenie z uśpienia bez sieci i panel z zapisaną siecią, która nie działa: nigdy sam (tylko ◀ 2 s).
    if (w == PowodWybudzenia::START && startZasilania() && k.pierwszaKonfiguracja && k.ssid.empty()) {
      log("pierwszy start bez sieci: sieć panelu sama (R-KONF-07)");
      otworzSiecPaneluP07(Ekran::DZIEN, false);
      ustawEkran(ap.aktywna ? Ekran::SIEC_PANELU : Ekran::DZIEN);
      r.ekranOd = teraz();
    }
    DecyzjaNocy dn = nocTeraz(true);  // pierwsza decyzja tej nocy: zapis w NVS przed rysowaniem
    bool rysuj = rysowacPoWybudzeniu(w, zmiana, zegar(), minutaDoby(), dn.rysujPoZegarze);
    // (R-PRZ-19): w nocy po czasie cofnięcia ten sam ekran potwierdzenia jeszcze raz, bez podpisu cofnięcia
    if (w == PowodWybudzenia::ZEGAR && zegar() &&
        koniecCofnieciaTeraz(ekran(), (long)(int32_t)(teraz() - r.ekranOd), minutaDoby()))
      rysuj = true;
    // wersja do sprawdzenia rysuje zawsze (samosprawdzenie: ekran się rysuje, T-WYM-41)
    if (samosprawdzenieWymagane(a.uruchomienie.stanWersji())) rysuj = true;
    if (otworzycOknoSerwisowe(ekran())) {
      otworzOknoAlboZamknijSiec();
      otaDo = ms() + OKNO_SERWISOWE_MS;
    }
    if (rysuj) pokaz();
    else log("slot nocny: ekran bez zmian");
    if (pobranieRano) pobierzRanoPoPrzycisku();  // R-PRZ-22: po narysowaniu akcji przycisku
    if (!samosprawdzenie(rysuj)) return;
    if (u07Przerysuj && ekran() == Ekran::AKTUALIZACJA) {  // wynik po oznaczeniu wersji jako dobrej
      u07Przerysuj = false;
      pokaz();
    }
    automatPoRysowaniu(w);  // R-KONF-45: po rysowaniu, gdy wiadomo, czy na ekranie jest sprawa A albo B
    if (koniecPoRestarcie) return;
    a.przyciski.dioda(false);
    czuwanie(w);
    if (koniecPoRestarcie) return;
    porownanieNaKoniec(w);  // tryb porównawczy (R-DANE-60): po rysowaniu i czuwaniu, przed snem
    spij();
  }

  // Czuwanie po rysowaniu (7.1 krok 13, T-WYM-20): po wybudzeniu przyciskiem 20 s, na ekranie testu z oknem
  // serwisowym 10 min (src/plan.h czuwanieMs()); po zegarze i starcie jeden obieg bez czekania: przycisk wciśnięty
  // w tej chwili obsłużony jak budzący (z czuwaniem 20 s; zacięty dopiero po ZACIECIE_MS, R-PRZ-09) i komendy `Z`, `O`
  // z portu serwisowego.
  void czuwanie(PowodWybudzenia w) {
    uint32_t t0 = ms();
    PowodWybudzenia tryb = w;
    bool komendy = false;  // komenda `W` albo `?` z portu trzyma czuwanie CZUWANIE_MS od ostatniej (obrazki dnia)
    for (;;) {
      odnow();
      if (otaAktywne) a.siec.obsluzOknoSerwisowe();  // wgrywanie blokuje tu do końca, potem restart
      obsluzRaportB13();  // serwer raportu trzyma panel w czuwaniu do 15 min (7.7)
      obsluzFormularzP07();  // serwer formularza tak samo (R-KONF-15)
      obsluzSiecPaneluP07();  // sieć panelu tak samo (R-KONF-29)
      int c = a.port.znak();
      if (c == 'Z') zrzutPlikow();  // U2: zrzut plików przed uploadfs
      // T-WYM-87: plik zrzutu poza oknem przywracania odrzucony; obrazki dnia przyjmowane
      if (c == 'W') komendaW("W" + resztaWiersza(), false);
      if (c == '?')  // U3: odpowiedź na zapytanie narzędzia; przy „bez przywracania” z podpowiedzią dla narzędzia
        a.port.wiersz(bezPrzywracania() ? "przywracanie: poza oknem, bez przywracania (komenda W otworzy okno przy "
                                          "następnym starcie zasilania)"
                                        : "przywracanie: poza oknem");
      if (c == 'W' || c == '?') {
        komendy = true;
        t0 = ms();
      }
      if (c == 'O') {               // okno wgrywania przez Wi-Fi bez przycisków (do testów)
        if (ap.aktywna) zamknijSiecPaneluP07();  // najpierw sieć panelu i serwer formularza
        ustawEkran(Ekran::TEST);
        r.ekranOd = teraz();
        if (!a.siec.polaczona()) odczyt(false);
        otworzOknoAlboZamknijSiec();
        t0 = ms();
        otaDo = t0 + OKNO_SERWISOWE_MS;
        pokaz();
      }
      Przycisk p = wcisniety();
      if (p != Przycisk::BRAK) {
        PowodWybudzenia przed = tryb;
        if (tryb != PowodWybudzenia::PRZYCISK) log("przycisk wciśnięty przed snem: obsługa jak przycisk budzący");
        tryb = PowodWybudzenia::PRZYCISK;
        przyciskWCzuwaniu = true;
        a.przyciski.dioda(true);
        bool dlugo = false;
        int skrot = przytrzymanieLubSkrot(p, ms(), dlugo);
        if (skrot == 1) p = Przycisk::PRAWY;
        PoNacisnieciu pn;
        if (skrot != 2) pn = obsluz(p, dlugo, false);
        if (koniecPoRestarcie) return;  // instalacja z U-07 zakończona restartem
        czekajNaPuszczenie();  // zacięty przycisk nie blokuje snu (U4)
        a.przyciski.wyczyscKolejke();  // to naciśnięcie obsłużone z odczytu stanu: nie wraca z kolejki
        // trzymany co najmniej ZACIECIE_MS: zacięcie, nie naciśnięcie, więc bez czuwania 20 s
        if (zaciete & (1u << (int)p)) tryb = przed;
        t0 = ms();
        // ekran testu: świeży stan sieci (tylko przy wejściu na test); Wi-Fi zostaje tylko dla okna serwisowego
        wykonajPobranie(pn.pobranie, otaAktywne || otworzycOknoSerwisowe(ekran()));
        if (otworzycOknoSerwisowe(ekran())) {
          otworzOknoAlboZamknijSiec();
          otaDo = t0 + OKNO_SERWISOWE_MS;
        }
        // R-KON-37: w nocy naciśnięcie bez akcji nie przerysowuje ekranu
        if (rysowacPoNacisnieciu(pn.zmiana, zegar(), minutaDoby())) pokaz();
        else log("slot nocny: naciśnięcie bez akcji, ekran bez zmian");
        a.przyciski.dioda(false);
      } else if (nacisnieciaZKolejki()) {
        tryb = PowodWybudzenia::PRZYCISK;
        przyciskWCzuwaniu = true;
        t0 = ms();
      }
      bool oknoOta = otaAktywne && ekran() == Ekran::TEST;
      uint32_t limit = czuwanieMs(tryb, oknoOta);
      if (komendy && limit < CZUWANIE_MS) limit = CZUWANIE_MS;
      if (ms() - t0 >= limit && !rap.serwer && !frm.serwer && !ap.aktywna) break;
      czekaj(OBIEG_CZUWANIA_MS);
    }
  }

  // Przytrzymanie przycisku `p` wciśniętego od `od` (ms) albo skrót ◀ + ▶ razem ≥ 2 s (R-PRZ-21,
  // 06.11.2026): test sprzętu U-05 z oknem wgrywania, tą samą drogą co U-03 ▶ 2 s. Skrót rozpoznany, gdy
  // w czasie trzymania ◀ albo ▶ dojdzie drugi z nich, więc zwykłe naciśnięcie nie czeka. Zwraca 0 = zwykłe naciśnięcie
  // (`dlugo` jak przytrzymany()), 1 = skrót (ekran U-03, dalej obsługa jak ▶ 2 s), 2 = oba puszczone przed 2 s
  // (bez akcji).
  int przytrzymanieLubSkrot(Przycisk p, uint32_t od, bool& dlugo) {
    const uint8_t oba = (uint8_t)((1u << (int)Przycisk::LEWY) | (1u << (int)Przycisk::PRAWY));
    const uint8_t bit = (uint8_t)(1u << (int)p);
    bool lewyAlboPrawy = p == Przycisk::LEWY || p == Przycisk::PRAWY;
    for (;;) {
      uint8_t b = a.przyciski.wcisniete();
      if (lewyAlboPrawy && (b & oba) == oba) break;
      Przytrzymanie s = przytrzymanie(od, ms(), (b & bit) != 0);
      if (s != Przytrzymanie::TRWA) {
        dlugo = s == Przytrzymanie::DLUGO;
        return 0;
      }
      czekaj(5);
    }
    uint32_t odOba = ms();
    while ((a.przyciski.wcisniete() & oba) == oba) {
      odnow();
      if (ms() - odOba >= PROG_PRZYTRZYMANIA_MS) {
        log("skrót ◀ + ▶ 2 s: test sprzętu z oknem wgrywania (R-PRZ-21)");
        ustawEkran(Ekran::FORMULARZ);  // dalej jak ▶ 2 s na U-03
        dlugo = true;
        return 1;
      }
      czekaj(5);
    }
    log("skrót ◀ + ▶ puszczony przed 2 s: bez akcji");
    dlugo = false;
    return 2;
  }

  // Kolejka naciśnięć (spec 5.6.1): naciśnięcia z czasu rysowania po kolei, ekran rysowany raz, na końcu. Tylko
  // nawigacja (zKolejkiWolno); pierwsze naciśnięcie poza nią kończy kolejkę i odrzuca resztę, bo ekranu, na który
  // trafiłoby, nikt jeszcze nie widział. Zwraca true, gdy było co najmniej jedno naciśnięcie z kolejki.
  bool nacisnieciaZKolejki() {
    int nr = 0;
    uint32_t trwalo = 0;
    int wykonane = 0, pominiete = 0;
    bool zmiana = false;
    while (a.przyciski.zKolejki(nr, trwalo)) {
      if (nr < (int)Przycisk::LEWY || nr > (int)Przycisk::PRAWY) continue;
      Przycisk q = (Przycisk)nr;
      bool dlugo = trwalo >= PROG_PRZYTRZYMANIA_MS;
      if (!zKolejkiWolno(ekran(), dlugo)) {
        pominiete = 1;
        while (a.przyciski.zKolejki(nr, trwalo)) pominiete++;
        break;
      }
      if (wykonane == 0) a.przyciski.dioda(true);
      zKolejkiTeraz = true;
      PoNacisnieciu pn = obsluz(q, false, false);
      zKolejkiTeraz = false;
      zmiana = zmiana || pn.zmiana;
      wykonane++;
      wykonajPobranie(pn.pobranie, otaAktywne || otworzycOknoSerwisowe(ekran()));
      if (otworzycOknoSerwisowe(ekran())) {
        otworzOknoAlboZamknijSiec();
        otaDo = ms() + OKNO_SERWISOWE_MS;
      }
    }
    if (wykonane == 0 && pominiete == 0) return false;
    log("kolejka naciśnięć: %d wykonane, %d pominięte (ekran %d)", wykonane, pominiete, (int)ekran());
    if (wykonane > 0) {
      if (rysowacPoNacisnieciu(zmiana, zegar(), minutaDoby())) pokaz();
      else log("slot nocny: naciśnięcie bez akcji, ekran bez zmian");
      a.przyciski.dioda(false);
    }
    return true;
  }

  // ---------- tryb awaryjny (T-WYM-83, R-AW-39; spec funkcjonalna U-06, U-05) ----------
  void rysujU06() {
    TekstyU06 t = tekstyU06();
    TrescEkranu tr;
    tr.uklad = Uklad::OGOLNY;
    tr.tytul = t.tytul;
    tr.powod = t.powod;
    tr.krok = t.krok;
    tr.ramkaPrzerywana = true;  // ton „brak danych” (5.4.3)
    tr.stopkaLewo = t.stopka;
    rysujEkranDanych(plotno(), Naglowek{"", ""}, tr);
    odnow();
    a.wyswietlacz.odswiez();
    r.klatkaOd = 0;  // rysowanie poza pokaz: skrót klatki nieważny
    odnow();
    log("tryb awaryjny: U-06 narysowany");
  }

  // U-05 w trybie awaryjnym (spec funkcjonalna U-05, T-WYM-83): układ kanwy z jedną kolumną, wiersze tylko wersja ze
  // slotem i przyczyna trybu, w ramce stan okna serwisowego, stopka tylko `◀ wróć`; pełny identyfikator wersji i liczba
  // startów po błędzie w logu portu.
  void rysujU05Awaryjny(const ZnacznikAwaryjny& z, bool okno) {
    otaAktywne = okno;
    otaDo = ms() + OKNO_SERWISOWE_MS;
    DaneTestu d;
    d.awaryjny = true;
    d.lewa = {
        {"Oprogramowanie", wersjaWydania(a.uruchomienie.wersja()) + " (" + a.uruchomienie.slot() + ")"},
        {"Tryb awaryjny", dopasujWartosc("Tryb awaryjny", {nazwaPrzyczyny(z.przyczyna)})},
    };
    d.ramkaLewo = ramkaOkna();
    d.ramkaPrawo = "hasła i dane terapii nie są pokazywane";
    log("U-05 · Wersja: %s · slot %s", a.uruchomienie.wersja(), a.uruchomienie.slot().c_str());
    log("U-05 · Tryb awaryjny: %u starty po błędzie (%s)", (unsigned)STARTOW_DO_AWARYJNEGO, nazwaPrzyczyny(z.przyczyna));
    rysujTest(plotno(), d);
    odnow();
    a.wyswietlacz.odswiez();
    r.klatkaOd = 0;  // rysowanie poza pokaz: skrót klatki nieważny
    odnow();
  }

  // Zwraca true po wyjściu z trybu awaryjnego (↻ 2 s na U-06): wołający robi zwykły start od nowa. Inaczej sen
  // z budzeniem tylko przyciskami (albo powrót do poprzedniej wersji, gdy niesprawdzona) i false.
  bool trybAwaryjny(const DecyzjaStartu& d, const ZnacznikAwaryjny& z, PowodWybudzenia w) {
    log("tryb awaryjny (R-AW-39): %u starty po błędzie, bez plików i bez sieci", (unsigned)d.poBledzie);
    // znacznik przed rysowaniem: błąd w rysowaniu U-06 nie daje pętli (u06Narysowane w tym samym zapisie)
    if (d.zapiszZnacznik) a.nvs.ustawTekst(NVS_PRZESTRZEN, NVS_KLUCZ_AWARYJNY, zapiszZnacznik(d.znacznik));
    EkranAwaryjny e = EkranAwaryjny::U06;
    bool okno = false, wifi = false;
    auto wlaczEkran = [&]() {
      if (!ekranWlaczony) a.wyswietlacz.wlacz();
      ekranWlaczony = true;
    };
    // akcja przycisku; true = wyjście z trybu awaryjnego
    auto wykonajAkcje = [&](AkcjaAwaryjna akc) {
      switch (akc) {
        case AkcjaAwaryjna::TEST_SPRZETU: {
          e = EkranAwaryjny::U05;
          bool przerwane = false;
          wifi = a.siec.polacz(LIMIT_WIFI_MS, [this]() {
            odnow();
            return false;
          }, przerwane);
          okno = wifi && a.siec.otworzOknoSerwisowe();
          wlaczEkran();
          rysujU05Awaryjny(z, okno);
          return false;
        }
        case AkcjaAwaryjna::WROC:
          if (wifi) a.siec.wylacz();
          wifi = okno = false;
          e = EkranAwaryjny::U06;
          wlaczEkran();
          rysujU06();
          return false;
        case AkcjaAwaryjna::WYJDZ:
          if (wifi) a.siec.wylacz();
          a.nvs.usun(NVS_PRZESTRZEN, NVS_KLUCZ_AWARYJNY);
          wyjscieZAwaryjnego(a.uruchomienie.licznik(), wersjaId);
          log("tryb awaryjny: wyjście po ↻ 2 s, zwykły start od nowa");
          return true;
        case AkcjaAwaryjna::ODSWIEZ:  // ↻ na U-05: stan na ekranie od nowa
          rysujU05Awaryjny(z, okno);
          return false;
        case AkcjaAwaryjna::NIC: break;
      }
      return false;
    };
    if (w == PowodWybudzenia::PRZYCISK) {
      Przycisk p = przyciskBudzacy(a.uruchomienie.przyciskiBudzace());
      bool dlugo = przytrzymany(p, 0);
      a.dzwiek.pisk(dlugo ? 2 : 1);
      if (wykonajAkcje(nacisniecieAwaryjne(e, p, dlugo))) return true;
      czekajNaPuszczenie();
    } else if (d.rysujU06) {
      wlaczEkran();
      rysujU06();
    }
    a.przyciski.dioda(false);
    uint32_t t0 = ms();
    for (;;) {
      odnow();
      if (ms() - t0 >= czasCzuwaniaMs(okno)) break;
      if (okno) a.siec.obsluzOknoSerwisowe();
      Przycisk p = wcisniety();
      if (p != Przycisk::BRAK) {
        a.przyciski.dioda(true);
        bool dlugo = przytrzymany(p, ms());
        a.dzwiek.pisk(dlugo ? 2 : 1);
        if (wykonajAkcje(nacisniecieAwaryjne(e, p, dlugo))) return true;
        czekajNaPuszczenie();
        t0 = ms();
        a.przyciski.dioda(false);
      }
      czekaj(OBIEG_CZUWANIA_MS);
    }
    if (e == EkranAwaryjny::U05) wykonajAkcje(AkcjaAwaryjna::WROC);  // po 10 min okno zamknięte, U-06 (5.6.2)
    if (wifi) a.siec.wylacz();
    PlanSnu plan = planSnuAwaryjny(a.przyciski.wcisniete());  // budzi tylko przycisk (R-AW-39)
    log("tryb awaryjny: sen do naciśnięcia przycisku");
    przedSnem(plan);
    return false;
  }
};

}  // namespace

void Dyrygent::wybudzenie() {
  teksty_ = TekstyWybudzenia();
  Przebieg p(a_, k_, teksty_);
  p.wykonaj();
  p.tekstyKoncowe();
}
