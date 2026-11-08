// SPDX-License-Identifier: GPL-3.0-or-later
// Decyzje jednego wybudzenia w rdzeniu (spec techniczna 13.1;
// ADR-0021 pkt 5): kiedy pobierać, który ekran po starcie, czy rysować, czas czuwania i snu, przyciski, decyzje
// przebiegu pobrania z AirBridge i wiersz logu pamięci. Bez Arduino: wszystko jako dane.
//
// Gdzie decyzja różni się od spec docelowej, komentarz mówi, w którym kroku 13.1 to się zmienia.
// Testy charakteryzujące: test/testy_wybudzenie.cpp.
#pragma once
#include <cmath>
#include <cstdint>
#include <string>
#include <type_traits>
#include <vector>
#include "airbridge.h"
#include "czynniki.h"
#include "dobranoc.h"
#include "humor.h"
#include "nawigacja.h"
#include "otoczenie.h"
#include "sypialnia.h"

// Powód wybudzenia z punktu widzenia rdzenia (adapter mapuje esp_sleep_get_wakeup_cause()):
// ZEGAR = ESP_SLEEP_WAKEUP_TIMER, PRZYCISK = ESP_SLEEP_WAKEUP_EXT1, START = każdy inny (zasilanie, restart, wgranie).
enum class PowodWybudzenia : uint8_t { START = 0, ZEGAR = 1, PRZYCISK = 2 };

// Bity przycisków w maskach (1 << (int)Przycisk): LEWY = 0x02, SRODEK = 0x04, PRAWY = 0x08.
const uint8_t BIT_LEWY = 1u << (int)Przycisk::LEWY;
const uint8_t BIT_SRODEK = 1u << (int)Przycisk::SRODEK;
const uint8_t BIT_PRAWY = 1u << (int)Przycisk::PRAWY;
const uint8_t WSZYSTKIE_PRZYCISKI = BIT_LEWY | BIT_SRODEK | BIT_PRAWY;

// Stałe czasu wybudzenia.
const uint32_t OKRES_SNU_S = 30 * 60;          // odświeżanie co 30 min (do kroku 2)
const uint32_t CZUWANIE_MS = 20000;            // czuwanie na przycisk po rysowaniu (dziś po każdym wybudzeniu)
const uint32_t OKNO_SERWISOWE_MS = 10UL * 60 * 1000;  // okno serwisowe (wgrywanie przez Wi-Fi) z ekranu testu
const uint32_t PROG_PRZYTRZYMANIA_MS = 2000;   // R-PRZ-08 [PRÓG]
const uint32_t ZACIECIE_MS = 10000;            // U4: przycisk wciśnięty dłużej = zacięty (T-WYM-29)
const uint32_t ZAPAS_POWROTU_S = 2;            // sen do powrotu po 5 min + 2 s (R-PRZ-02)
const uint32_t ZAPAS_POLGODZINY_S = 5;         // sen do pełnej albo wpół godziny + 5 s
const int ZALEGLE_NA_WYBUDZENIE = 4;           // najwyżej tyle zaległych dób na jedno wybudzenie
const uint32_t MIN_NA_RAPORT_ZALEGLY_MS = 20000;  // najmniej tyle budżetu na raport kolejnej zaległej doby
const uint32_t CZAS_ZNANY_OD = 1700000000;     // czas unix większy = zegar ustawiony

// Stan AirBridge od v2.2.0 z ostatniego odczytu (RTC, kopia w NVS „noc”/„ab”). Układ pól bez
// zmian względem prototypu: NVS ma zapis bajtów tej struktury (sizeof i kolejność pól muszą zostać, inaczej po
// zaniku zasilania stan nie wraca). Liczby nocy są w historii.
struct StanAB {
  uint32_t magia = 0;
  bool ma = false;          // był udany odczyt /api/status
  uint32_t pobrano = 0;     // epoch ostatniego udanego odczytu
  char wersja[24] = {};     // `version`, np. "v2.2.1-25-gebbd145"
  char tryb[16] = {};       // `mode`, np. "ASVAuto"
  int8_t rop = 0;           // 0 czuwanie, 1 terapia, 2 przywracanie, 3 oszczędzanie, 4 kalibracja, 5 aktualizacja; -1 nieznany
  char prefiks[16] = {};    // `edf_prefix`: w terapii start bieżącej sesji RRRRMMDD_HHMMSS, poza terapią ""
  bool zablokowane = false; // terapia trwa albo `report_state` = "blocked": bez zapytań o raporty
  bool wBudowie = false;    // ostatnia noc w budowie (R-AW-15, 5.1.4)
  int zalegle = 0;          // doby z AirBridge gotowe do pobrania; -1 = nie wiadomo
  int czekajace = 0;        // doby czekające po nieudanej próbie (do ekranu testu)
  uint32_t bladOd = 0;      // epoch pierwszej nieudanej próby od ostatniego udanego odczytu (E-01); 0 = brak
};
static_assert(sizeof(StanAB) == 84, "układ StanAB jak w prototypie (zapis bajtów w NVS)");
const uint32_t MAGIA_AB = 0xC0A70005;  // znacznik ważnego bloku StanAB w RTC

// Stan pytania „Co było inaczej?” między wybudzeniami.
struct StanPytania {
  char data[11] = {};
  uint8_t kursor = 0, zaznaczone = 0;
  int16_t termin = 0;  // -1: pytanie na żądanie z K1
  bool innaNoc = false;
  bool ruszone = false;  // w siatce zaznaczono albo wybrano pozycję; bez tego ↻ 2 s nic nie zapisuje
};

// Stan między wybudzeniami w pamięci RTC (ekran, chwila jego wejścia, licznik wybudzeń, ostatni odczyt, stan
// AirBridge, pytanie, fakty nocy, bufor sypialni, otoczenie). Po starcie innym niż wybudzenie z uśpienia
// wartości domyślne (adapter ESP32 tworzy blok od nowa, jak bootloader zmienne RTC_DATA_ATTR).
// Najdłuższy czas bez pełnego odświeżenia e-ink przy klatce bez zmian: po nim klatka
// identyczna z wyświetloną i tak odświeża ekran. [PRÓG] 24 h z zaleceń producentów ekranów e-ink („refresh at least
// once every 24 hours”, instrukcje Waveshare); dla ekranu E1001 [DOMYSŁ], do sprawdzenia w karcie katalogowej.
const uint32_t KLATKA_NAJDLUZEJ_S = 24 * 3600;

struct StanRtc {
  uint8_t ekran = 0;                 // (uint8_t)Ekran
  uint32_t ekranOd = 0;              // czas unix ostatniego naciśnięcia (R-PRZ-02)
  uint32_t wybudzen = 0;             // licznik wybudzeń (ekran testu)
  bool ostatniOdczytOk = false;      // wynik ostatniego pobrania (S5 po wybudzeniu przyciskiem)
  int32_t aktywnyPoprzedniMs = -1;   // czas aktywności poprzedniego wybudzenia (pomiar baterii)
  StanAB ab;                         // stan AirBridge (magia ≠ MAGIA_AB: odtworzenie z NVS)
  StanPytania pyt;                   // pytanie „Co było inaczej?”
  FaktyNocy faktyNocy;               // fakty ekranu nocnego z pierwszego rysowania tej nocy (dobranoc.h)
  BuforSypialni buforSyp{};          // odczyty czujnika z okna nocy (sypialnia.h)
  Otoczenie oto{};                   // ostatnia odpowiedź lokalnego źródła otoczenia (magia ≠ OTO_MAGIA: od nowa)
  // Minimum wolnej sterty i zapasu stosu z wybudzeń jednej doby (ekran testu): doba jako nrDnia daty
  // lokalnej + 1 (0 = brak pomiaru albo zegar nieznany).
  int32_t pamiecDoba = 0;
  uint32_t pamiecSterta = 0, pamiecStos = 0;
  // Krok 2 (src/plan.h, R-DANE-18, R-AW-34): ostatnia próba AirBridge skończyła się odrzuconym logowaniem
  // (HTTP 401). Tylko w RTC, nie w StanAB (NVS trzyma bajty StanAB); po zaniku zasilania false do następnej próby.
  bool logowanieOdrzucone = false;
  // Dla planu (src/plan.h WejsciePlanu::obsluzoneDo): czas unix ostatniej chwili, do której
  // terminy planu wykonało wybudzenie zegarem albo start; 0 = nieznana (po zaniku zasilania, bez zegara).
  uint32_t obsluzoneDo = 0;
  // T-WYM-29: bity przycisków uznanych za zacięte (wciśnięte co najmniej ZACIECIE_MS)
  // w chwili zasypiania; po wybudzeniu zacięte zostają tylko te, które są nadal wciśnięte.
  uint8_t zaciete = 0;
  // ADR-0013: czas unix ostatniej udanej synchronizacji NTP (0 = brak od startu).
  uint32_t ntpOstatnio = 0;
  // R-EKR-60: czas unix ostatniego narysowania ekranu dnia ze stopką R-EKR-10; 0 = ostatnio
  // narysowany ekran dnia bez przerysowań (S5, S10, ekran nocny) albo nieznany. Plan liczy z niego termin co 2 h.
  uint32_t dzienRysowanyEp = 0;
  // Skrót CRC-32 bufora klatki ostatnio naprawdę wyświetlonej (pełne
  // odświeżenie e-ink) i czas unix tego odświeżenia; klatkaOd = 0: skrót nieważny (start inny niż wybudzenie z uśpienia,
  // zmiana ekranu, rysowanie poza Przebieg::pokaz(), bez zegara). Klatka identyczna z wyświetloną nie odświeża e-ink.
  uint32_t klatkaSkrot = 0;
  uint32_t klatkaOd = 0;
  // R-EKR-60: tryb ('K' karta, 'D' ekran danych; 0 = nieznany) i linia uwagi (0 brak,
  // 1 zaproszenie R-PIE-08, 2 E-08) ekranu dnia z ostatniego narysowania; przerysowanie bez pobrania ich nie zmienia.
  char dzienTryb = 0;
  uint8_t dzienLinia = 0;
  // Slot dzienny (src/plan.h WejsciePlanu, R-KON-41): czas unix pierwszego zapisu rekordu kompletnego
  // ostatniej nocy tej doby (import nocy), startu slotu dziennego (wybudzenie z powodem startu albo naciśnięcie)
  // i naciśnięcia na ekranie dnia po imporcie, które zaczęło slot; 0 = nie było. W RTC; po zaniku zasilania chwila
  // importu z rekordu nocy (pole `import`), a przy pustym polu zastępnik z NVS
  // (src/dyrygent.cpp Przebieg::zastepnikImportu()).
  uint32_t importNocyEp = 0;
  uint32_t slotDziennyEp = 0;
  uint32_t nacisniecieRanoEp = 0;
  // (magazyn danych): doba (jak pamiecDoba) ostatniego sprawdzenia, czy dziennik wybudzeń /log/wybudzenia.csv
  // wymaga przycięcia (raz na dobę, bo pierwszy wiersz danych stoi za nagłówkiem); 0 = jeszcze nie.
  int32_t wybudzeniaDoba = 0;
  // (U-05, R-KONF-41): ostatnie połączenie z Wi-Fi (adres IP panelu, nazwa sieci, sygnał w dBm, czas unix;
  // wifiIp pusty = nie było od startu). Bez połączenia U-05 pokazuje je z godziną w nawiasie.
  char wifiIp[16] = {};
  char wifiSsid[33] = {};
  int16_t wifiRssi = 0;
  uint32_t wifiEp = 0;
  // odczyt czujnika w obudowie (°C, %) w stopce ostatnio naprawdę wyświetlonej klatki; wybudzenie samej
  // oceny wieczornej bez Wi-Fi (`slot_wieczorny_bez_sieci`) rysuje stopkę z tych wartości, więc e-ink odświeża się
  // tylko przy zmianie ekranu (jak R-EKR-60 dla stopki: co 2 h, tu wcale). NAN = brak.
  float stopkaTemp = NAN, stopkaWilg = NAN;
  // temperatura za oknem w stopce slotu dziennego tej klatki (z prognozy albo źródła otoczenia); ocena o pełnej
  // godzinie rysuje stopkę z niej i z stopkaTemp (06.11.2026, R-KON-42). NAN = brak albo inny ekran.
  float stopkaZaOknem = NAN;
  // M-25 (E-05): wygładzony procent baterii i surowy procent poprzedniego pomiaru
  // (src/bateria.h wygladzProcent()); -1 = brak (po zaniku zasilania od nowa z pierwszego pomiaru).
  int8_t bateriaProc = -1, bateriaSurowy = -1;
};
// StanRtc leży w pamięci RTC i przetrwa głęboki sen tylko jako ciąg bajtów: bez wskaźników, std::string i wirtualnych.
static_assert(std::is_trivially_copyable<StanRtc>::value, "StanRtc musi być trywialnie kopiowalny (pamięć RTC)");

// ---------- czas ----------

// Czy zegar systemowy jest ustawiony. Przyjmuje czas unix; zwraca t > CZAS_ZNANY_OD.
bool zegarZnany(uint32_t teraz);

// ---------- pobieranie: kiedy ----------

struct Pobranie {
  bool pobierz = false;          // odczyt AirBridge i źródła otoczenia
  bool przerywalne = false;      // naciśnięcie po puszczeniu przerywa pobieranie (abPrzerywanie(true))
  bool poranekCzynnikow = false; // po odczycie poranna decyzja o pytaniu
};

// Pobranie na początku wybudzenia, przed obsługą przycisku (T-WYM-24). Przyjmuje powód wybudzenia.
// Zwraca: ZEGAR i START → pobierz, nieprzerywalne, z porannym pytaniem; PRZYCISK → nic (tylko zmiana widoku).
Pobranie pobranieNaStarcie(PowodWybudzenia p);

// Pobranie po naciśnięciu przycisku. Przyjmuje przejście z nacisniecie(), ekran
// przed naciśnięciem i czy to przycisk budzący (true) czy naciśnięcie w czuwaniu (false).
// Zwraca: Akcja::PONOW_POBRANIE (↻ na S5, R-PRZ-06) → pobierz przerywalnie z porannym pytaniem; wejście na ekran
// testu → pobierz przerywalnie bez porannego pytania (przy przycisku budzącym także, gdy test już był na ekranie;
// w czuwaniu tylko, gdy przed naciśnięciem testu nie było); inaczej nic.
Pobranie pobraniePoNacisnieciu(const Przejscie& pr, Ekran przed, bool przyciskBudzacy);

// ---------- ekran i rysowanie ----------

// Ekran po wczytaniu stanu i odczycie, przed rysowaniem. Przyjmuje powód, ekran z RTC (dla
// PRZYCISK: już po nacisniecie()), czy zegar znany, sekundy od ostatniego naciśnięcia i minutę doby.
// Zwraca: START → DZIEN; ZEGAR → DZIEN, gdy przy znanym zegarze sekundDoPowrotu() == 0 (R-PRZ-02, R-PRZ-18), oraz
// DZIEN zamiast TEST (okno serwisowe nie otwiera się po zegarze); inaczej ekran z RTC; PRZYCISK → ekran z RTC.
Ekran ekranPoStarcie(PowodWybudzenia p, Ekran zRtc, bool zegar, long sekundOdNacisniecia, int minuta);

// Czy rysować po wybudzeniu (U1, R-KON-37). Przyjmuje powód, czy naciśnięcie budzące coś
// zmieniło (obsluz()), czy zegar znany, minutę doby i DecyzjaNocy::rysujPoZegarze tego wybudzenia.
// Zwraca: START → zawsze; ZEGAR w slocie nocnym przy znanym zegarze → tylko rysujPoZegarze (pierwsza decyzja S9),
// poza slotem albo bez zegara → tak; PRZYCISK → nie tylko wtedy, gdy naciśnięcie nic nie zmieniło, zegar znany,
// trwa slot nocny i to nie jest pierwsza decyzja S9.
bool rysowacPoWybudzeniu(PowodWybudzenia p, bool zmiana, bool zegar, int minuta, bool rysujPoZegarze);

// Czy przerysować po naciśnięciu w czuwaniu (R-KON-37). Zwraca: zmiana albo zegar
// nieznany albo poza slotem nocnym.
bool rysowacPoNacisnieciu(bool zmiana, bool zegar, int minuta);

// Czy otworzyć okno serwisowe (nasłuch wgrywania). Przyjmuje ekran po ekranPoStarcie() albo
// po naciśnięciu. Zwraca e == Ekran::TEST. Po zegarze nigdy, bo ekranPoStarcie() zmienia TEST na DZIEN.
bool otworzycOknoSerwisowe(Ekran e);

// „Ekran dnia trwa” na noc (R-KON-35) przy decyzji o S9. Przyjmuje ekran na panelu, wieczór
// slotu nocnego z wieczorNocy() ("" poza slotem), stan nocy z NVS i czy ekran dnia to S5 (brakDanych()).
// Zwraca: ekran DZIEN i wieczór niepusty i inny niż stan.wieczor (pierwsza decyzja tej nocy) i S5.
bool ekranDniaTrwa(Ekran e, const std::string& wieczor, const StanNocy& stan, bool s5);

// ---------- czuwanie, sen, przyciski ----------

// Czas czuwania po rysowaniu. Przyjmuje, czy okno serwisowe jest otwarte i ekran to test.
// Zwraca OKNO_SERWISOWE_MS albo CZUWANIE_MS. Prototyp czuwa po każdym wybudzeniu, także zegarem (krok 2 to zmienia).
uint32_t czasCzuwaniaMs(bool oknoSerwisowe);

struct PlanSnu {
  bool budzZegarem = true;  // false: budzą tylko przyciski (tryb awaryjny, T-WYM-83)
  uint32_t sekund = 0;      // czas snu, gdy budzZegarem
  uint8_t przyciski = 0;    // bity przycisków, które budzą (0 = żaden, bez ext1)
};

// Plan snu. Przyjmuje: czy zegar znany, czas unix teraz, ekran, czas unix ostatniego naciśnięcia,
// minutę doby i bity przycisków wciśniętych w chwili zasypiania.
// Zwraca: bez zegara 1800 s; z zegarem do najbliższej pełnej albo wpół godziny + 5 s (1800 − teraz % 1800 + 5),
// skrócone do sekundDoPowrotu() + 2 s, gdy powrót istnieje i wypada wcześniej (R-PRZ-02; w slocie nocnym powrotu
// nie ma, R-PRZ-18); budzą przyciski niewciśnięte (wciśnięty w chwili zasypiania = zacięty, T-WYM-29).
PlanSnu planSnu(bool zegar, uint32_t teraz, Ekran e, uint32_t ekranOd, int minuta, uint8_t wcisniete);

// Plan snu w trybie awaryjnym (R-AW-39: budzi tylko przycisk). Przyjmuje bity przycisków wciśniętych w chwili
// zasypiania. Zwraca budzenie przyciskami niewciśniętymi, bez zegara; gdy wszystkie są wciśnięte (zacięte), zegar
// co OKRES_SNU_S, żeby panel nie spał do zaniku zasilania.
PlanSnu planSnuAwaryjny(uint8_t wcisniete);

// Przycisk budzący z maski ext1 (T-WYM-21). Przyjmuje bity przycisków z maski.
// Zwraca LEWY, gdy jest w masce, potem SRODEK, a w pozostałych przypadkach (także maska pusta) PRAWY.
Przycisk przyciskBudzacy(uint8_t maskaExt1);

enum class Przytrzymanie : uint8_t { TRWA, KROTKO, DLUGO };
// Rozstrzygnięcie przytrzymania (R-PRZ-08, T-WYM-21). Przyjmuje chwilę naciśnięcia
// (przy przycisku budzącym 0 = start programu), chwilę teraz w ms zegara procesora i czy przycisk jest wciśnięty.
// Zwraca KROTKO, gdy puszczony; DLUGO, gdy wciśnięty i minęło co najmniej PROG_PRZYTRZYMANIA_MS; inaczej TRWA.
Przytrzymanie przytrzymanie(uint32_t odMs, uint32_t terazMs, bool wcisniety);

// Wciśnięty przycisk z pominięciem zaciętych (U4, T-WYM-29). Przyjmuje bity wciśniętych w tej
// chwili i bity zaciętych (zmieniane). Sprawdza w kolejności LEWY, SRODEK, PRAWY: puszczony zdejmuje swój bit
// zaciętego, wciśnięty niezacięty jest wynikiem (kolejnych już nie sprawdza). Zwraca BRAK, gdy nic.
Przycisk wcisnietyPrzycisk(uint8_t wcisniete, uint8_t& zaciete);

// Po czekaniu na puszczenie najwyżej ZACIECIE_MS. Przyjmuje przycisk nadal
// wciśnięty po czekaniu (BRAK = puszczony) i bity zaciętych. Zwraca bity zaciętych z dopisanym tym przyciskiem.
uint8_t zacietePoCzekaniu(Przycisk nadalWcisniety, uint8_t zaciete);

// ---------- przebieg pobrania z AirBridge ----------

enum class PoStatusie : uint8_t {
  BRAK_ODPOWIEDZI,  // kod inny niż 200 (R-AW-12; 401 = logowanie odrzucone): nieudany odczyt, httpKod = kod
  E03,              // `/api/status` nieczytelny albo wersja AirBridge spoza zakresu (R-AW-02, Załącznik A pkt 4)
  ZABLOKOWANE,      // terapia trwa albo raporty zablokowane: bez listy i raportów, odczyt udany
  PRZETWARZA,       // moduł przetwarza pliki po terapii: bez listy, odczyt udany
  DALEJ,            // lista nocy i raport ostatniej nocy, gdy czegoś brakuje
};
// Decyzja po `/api/status`. Przyjmuje kod HTTP i ciało; wypełnia `st` (parsujStatus()). Zwraca jak wyżej, w tej
// kolejności sprawdzeń: kod, czytelność, wersja (tylko wersja czytelna i nieobsługiwana daje E03), blokada, przetwarzanie.
PoStatusie poStatusie(int kod, const std::string& tresc, StatusAB& st);

// Ostatnia noc „w budowie” przy blokadzie raportów. Przyjmuje, czy data dziś znana,
// minutę doby i czy noc z dziś jest w historii. Zwraca: data znana, przed 12:00 i brak nocy.
// Granica 12:00 to prototyp; spec R-DANE-35 ma 23:30 (zadanie, krok 4 z 13.1). Krok 1 przenosi bez zmiany.
bool wBudowiePrzyBlokadzie(bool dataZnana, int minuta, bool nocJest);
// To samo przy przetwarzaniu plików (krok 2a): bez granicy godziny. Zwraca data znana i brak nocy.
bool wBudowiePrzyPrzetwarzaniu(bool dataZnana, bool nocJest);

// Kod do ekranu i logu po nieudanej liście albo dobie: 200 z ciałem nieczytelnym albo urwanym
// to nieudana próba, nie E-03. Zwraca AB_KOD_URWANE dla 200, inaczej kod bez zmian.
int kodPoNieudanej(int kod);

// Koniec odpytywania joba. Przyjmuje ms od początku raportu, limit raportu w ms i ms do końca
// budżetu wybudzenia. Zwraca true, gdy uplynelo + AB_ODPYTYWANIE_MS >= limit albo zostalo <= AB_ODPYTYWANIE_MS.
bool koniecOdpytywania(uint32_t uplynelo, uint32_t limitMs, long zostaloMs);

struct WarunkiZaleglych {
  PowodWybudzenia powod = PowodWybudzenia::ZEGAR;
  Ekran ekran = Ekran::DZIEN;
  bool odczytUdany = false, listaOk = false, zablokowane = false, plikiOk = false, wifi = false;
  bool dataZnana = false;
  int minuta = 12 * 60;
};
// Czy dociągać zaległe doby po rysowaniu (R-DANE-06, R-DANE-26).
// Zwraca true tylko przy: powód inny niż PRZYCISK, ekran inny niż TEST, odczyt udany, lista z tego wybudzenia,
// raporty niezablokowane, pliki zamontowane, Wi-Fi połączone, data znana i poza slotem nocnym.
bool dociagacZalegle(const WarunkiZaleglych& w);

// Plan zaległych dób na to wybudzenie. Przyjmuje wynik planPobrania() (najnowsze
// najpierw) i datę dziś. Zwraca plan bez doby z datą poranka dziś (zostaje zwykłemu odczytowi), najwyżej
// ZALEGLE_NA_WYBUDZENIE pierwszych pozycji.
std::vector<long> planZaleglych(const std::vector<long>& plan, const std::string& dzis);

// Czy zacząć raport kolejnej zaległej doby. Zwraca zostaloMs >= MIN_NA_RAPORT_ZALEGLY_MS.
bool zaczacRaportZaleglej(long zostaloMs);

// Czy `/api/status` dał pola statusu (przy nieczytelnym statusie pola zostają z poprzedniego
// odczytu, przy wersji spoza zakresu są kopiowane i S5 pokazuje wersję). Przyjmuje wynik poStatusie() i jego `st`.
// Zwraca false tylko dla E03 z nieczytelnego statusu (parsujStatus() zostawił pustą wersję).
bool statusCzytelny(PoStatusie ps, const StatusAB& st);

// Początek błędu AirBridge do E-01 `nie odpowiada od`. Przyjmuje wynik odczytu, dotychczasowy
// początek (0 = brak), czy zegar znany i czas teraz. Zwraca 0 po udanym; po nieudanym dotychczasowy, a gdy go nie
// było i zegar znany, teraz.
uint32_t bladOdPoOdczycie(bool udany, uint32_t bladOd, bool zegar, uint32_t teraz);

// S5 kartą z budzikiem czy ekranem danych (U6). Zwraca false dla KOD_WIFI, KOD_E03 i 401.
bool kartaBraku(int httpKod);
// Terapia trwa: tylko z odczytu z tego wybudzenia. Zwraca odczytUdany && rop == 1.
bool terapiaTrwa(bool odczytUdany, int rop);

// Ziarno grupy kontrolnej, raz na panel (R-CZY-02). Przyjmuje wartość z NVS (0 = brak) i liczbę
// losową. Zwraca wartość z NVS, gdy niezerowa, inaczej losowa | 1 (nigdy 0).
uint32_t ziarnoPanelu(uint32_t zNvs, uint32_t losowa);

// ---------- ekran dnia i nawigacja ----------

// Kontekst mapy przycisków. Przyjmuje ekran na panelu, decyzję nocy bez zapisu (nocTeraz(false)),
// czy ekran dnia to S5 (brakDanych()) i S10 (synchronizacja()), linię zaproszenia (R-CZY-17) i czy jest ostatnia noc.
// Zwraca: nocny = S9 narysowany tej nocy (!dn.nowa && dn.nocny); s5 = ekran dnia, nie S9, i S5 albo S10 (↻ ponawia
// pobranie, R-PRZ-06); zaproszenie i ostatniaNoc bez zmian.
KontekstNaw kontekstNawigacji(Ekran e, const DecyzjaNocy& dn, bool s5, bool s10, bool zaproszenie, bool ostatniaNoc);

struct LiniaDnia {
  bool zaproszenie = false;  // `Co było inaczej? ↻ 2 s` (R-CZY-17)
  bool e08 = false;          // E-08 `co2_milczy` (grupa „dane”, raz na epizod)
  bool zapiszE08 = false;    // stan E-08 zmieniony: zapis w NVS
};
// Linia uwagi na ekranie dnia. Przyjmuje, czy zaproszenie jest aktywne (zaproszenieTeraz()),
// typ ekranu dnia, ostatnią odpowiedź źródła otoczenia, czas unix teraz, datę dziś i stan E-08 (zmieniany).
// Zwraca: zaproszenie tylko poza S5 (R-CZY-17 fail-path); bez zaproszenia i poza S5 E-08 według pokazE08() z odpowiedzią
// świeżą (OTO_SWIEZOSC_OSTRZEZEN_S) bez ostrzeżenia jako końcem epizodu; zapiszE08, gdy pokazE08() zmienił stan.
// `co2`: ekrany CO2 włączone w egzemplarzu (5.8.8); bez nich linii E-08 nie ma
LiniaDnia liniaDnia(bool zaproszenieAktywne, const std::string& typ, const Otoczenie& o, uint32_t teraz,
                    const std::string& dzis, StanE08& st, bool co2 = true);

struct PlanWyboru {
  bool zWpisu = false;  // to samo wyświetlenie dnia: tryb, scena i zdanie z dziennika
  Wybor wybor;          // przy zWpisu: wybór z wpisu (tryb 'D', gdy pojawiła się linia, R-HUM-17)
  bool dopisz = false;  // przy nowym wyborze: dopisać wpis do /humor.csv (brak wpisu z dziś)
};
// Karta czy ekran danych z dziennika. Przyjmuje wpis dziennika z dziś (nullptr = brak), typ
// ekranu i czy jest linia uwagi. Zwraca zWpisu przy wpisie i typie innym niż S5; inaczej nowy wybór (wybierzTryb()),
// z dopisaniem wpisu tylko, gdy wpisu nie było.
PlanWyboru planWyboru(const WpisHumoru* juz, const std::string& typ, bool jestLinia);

// R-PIE-08: linia zaproszenia liczy się jako pokazana dopiero, gdy była na ekranie. Zwraca linia && wpis z dziś
// istnieje && nie był jeszcze oznaczony.
bool oznaczycZaproszenie(bool linia, const WpisCzynnikow* zap);

// S10 kartą z (R-EKR-57). Zwraca: humor włączony, zdanie synchronizacji niepuste, scena
// B05 w firmware i karta się mieści.
bool kartaS10(Humor h, const std::string& zdanie, bool scenaB05, bool miesci);

// ---------- log pamięci ----------

const uint32_t PROG_STERTY_B = 64u * 1024;  // minimum wolnej sterty w wybudzeniu [PRÓG]
const uint32_t PROG_STOSU_B = 4u * 1024;    // zapas stosu zadania loop [PRÓG]

struct PamiecWybudzenia {
  uint32_t minSterta = 0;   // najmniejsza wolna sterta od startu programu (heap_caps_get_minimum_free_size)
  uint32_t zapasStosu = 0;  // najmniejszy zapas stosu zadania loop (uxTaskGetStackHighWaterMark)
};
// Czy pomiar mieści się w budżecie. Zwraca minSterta >= PROG_STERTY_B i zapasStosu >= PROG_STOSU_B.
bool pamiecWBudzecie(const PamiecWybudzenia& p);
// Wiersz logu wybudzenia z pamięcią, wypisywany tuż przed snem (port serwisowy). Zwraca
// `pamiec sterta_min={B} stos_zapas={B}`, a poza budżetem z dopiskiem ` poza_budzetem`.
std::string wierszPamieci(const PamiecWybudzenia& p);
