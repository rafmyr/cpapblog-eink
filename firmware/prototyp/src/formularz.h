// SPDX-License-Identifier: GPL-3.0-or-later
// Formularz konfiguracji na telefonie (konfiguracja z telefonu; spec 5.6.3 U-02, U-03, 5.6.4, 5.6.6, R-KONF-01, R-KONF-13…
// R-KONF-18, R-KONF-34, R-KONF-38, R-KONF-39; spec techniczna T-INT-07, T-KOMP-11, T-KOMP-12). Rdzeń bez gniazd i bez
// Arduino: strona HTML (bez zewnętrznych zasobów, po polsku, z ucieczką znaków), sesja z jednorazowym tokenem
// i limitem 15 min (ten sam mechanizm co raport, src/serwer_raportu.h), odczyt ciała `application/x-www-form-urlencoded`,
// walidacja pól według 5.6.4 i scalenie zmian z `/konfiguracja.txt` (reszta wierszy pliku zostaje bez zmian).
// formularz ma też miasto do pogody (lista 18 miast wojewódzkich z src/miasta.h, bez zapytania do internetu),
// ekrany CO2 (tylko ze źródłem otoczenia), humor, dźwięk, ciszę nocną, podpowiedź „co do poprawy” i czynniki w raporcie.
// Hasła (sieć domowa, AirBridge) nigdy nie wracają do przeglądarki: pole puste z napisem `ustawione`, puste pole przy
// zapisie = bez zmiany, pole wyboru `bez hasła` usuwa hasło (R-KONF-38, R-KONF-39). Formularz nie ma żadnego
// ustawienia aparatu (R-KONF-01, ADR-0002).
#pragma once
#include <cmath>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "dobry_start.h"
#include "sekrety.h"
#include "serwer_raportu.h"
#include "uklad_ekranow.h"

namespace formularz {

const int ELEMENTY = 2;  // maska, filtr (5.9.2; klucze `wymiana.*`, `okres.maska`, `okres.filtr_tyg`)
extern const char* const KLUCZE_ELEMENTOW[ELEMENTY];  // "maska", "filtr"
extern const char* const NAZWY_ELEMENTOW[ELEMENTY];   // "Maska", "Filtr"

// Wartości do pokazania w formularzu (obecny stan panelu), bez haseł.
struct Pola {
  // pola `bool` na końcu struktury (bez dziur między napisami; clang-analyzer-optin.performance.Padding)
  std::string ssid;
  std::string abAdres, abKonto;
  std::string profil = "prywatny";   // `publiczny` | `prywatny`
  std::string wymiana[ELEMENTY];     // RRRR-MM-DD albo ""
  std::string okres[ELEMENTY];       // maska: miesiące, filtr: tygodnie, "" = domyślny okres
  // R-MAS-17: termin następnej wymiany przy polu daty (sprzet::terminFormularza(), z datami po odpowiedzi „tak” na
  // `Wymieniona maska?`, R-MAS-13); ustawia wołający (dyrygent), zPliku() zostawia puste = bez wiersza terminu
  std::string termin[ELEMENTY];
  // (5.6.4). Wartości domyślne ze spec, gdy w pliku nie ma wiersza.
  std::string co2 = "tak";            // `tak` | `nie`
  std::string humor = "normalny";     // `wylaczony` | `oszczedny` | `normalny`
  std::string buzzer = "tak";         // `tak` | `nie`
  std::string ciszaOd = "22:00", ciszaDo = "08:00";  // GG:MM
  std::string podpowiedz = "tak";     // `tak` | `nie`
  std::string miasto;                 // "" = nie wybrano, albo nazwa z listy miast (src/miasta.h)
  std::string czynniki = "KL";        // kody zaznaczonych czynników raportu (bez wiersza: stan domyślny 5.13.7)
  std::string uwagaOMasce = "10";     // R-KONF-42 „Uwaga o masce od”, L/min 10–15 (klucz `uwaga_o_masce`)
  // R-DANE-66 (ADR-0046): limit ładowania baterii na kablu, klucz `ladowanie_limit`, domyślnie `nie` (brak wiersza)
  std::string ladowanieLimit = "nie";  // `tak` | `nie`
  // sekcja „Aktualizacje” (ADR-0038): automat z pliku (`aktualizacja_auto=tak`,
  // domyślnie `nie`); token wydań i hasło okna serwisowego tylko jako „ustawiony” (wartości nigdy do przeglądarki)
  std::string aktualizacjaAuto = "nie";  // `tak` | `nie`
  // R-KONF-62 (kanał wczesny, decyzja właściciela projektu z 23.11.2026): `aktualizacja_wczesna=tak`, domyślnie `nie` (bez wiersza)
  std::string aktualizacjaWczesna = "nie";  // `tak` | `nie`
  bool kluczWydan = false;               // token wydań zapisany w pamięci panelu (wołający ustawia z NVS)
  bool hasloOkna = false;                // hasło okna serwisowego w pamięci panelu (wołający ustawia z NVS)
  // R-KONF-59: numer wydania oprogramowania w nagłówku strony (`wersja {X}`); ustawia wołający (dyrygent, ten sam
  // napis co U-01), "" = bez wersji w nagłówku
  std::string wersja;
  // sekcja „Ekrany” (ADR-0045, R-KONF-60): układ kokpitu i ekranów dziennych z pliku (parsujUstawieniaDnia)
  UkladEkranow ekrany;
  // R-KON-77: „Wygaszacz ekranu” (klucz `dzien.wygaszacz`, pole `wygaszacz` tak|nie, domyślnie `tak`; jedyne miejsce
  // wyłączenia samego wygaszacza) i próg bezczynności (klucz `dzien.wygaszacz_min`, pole `wygaszacz_min`, 30–720, 180).
  // Pole „Obrazek dnia” (`pokaz_obrazek`, `obrazek` w `dzien.ukryte`) ukrywa obrazek w obu rolach.
  std::string wygaszacz = "tak";
  std::string wygaszaczMin = "180";
  // sekcja „Dobry start” (ADR-0049, R-DS-31): wartości kluczy `start.*` z pliku (ten sam parser co panel); nagłówek,
  // stan programu i liczby z pliku porad ustawia wołający (dyrygent, ustawStartFormularza())
  dobry_start::Ustawienia start;
  std::string startNaglowek = "wyłączony";
  int startZatrzymanyNa = 0;     // > 0: program zatrzymany w tej dobie programu (pytanie „dalej czy od nowa”, R-DS-32)
  int startMiejsc = 0;           // ostatnia doba programu z pliku porad (0 = brak)
  bool startAnkietaCzeka = false;
  bool startBezPorad = false;    // brak pliku porad (R-DS-36)
  bool hasloWifi = false;        // hasło zapisane: pole puste z napisem `ustawione`
  bool hasloAb = false;
  // R-MAS-19: nowy model maski zapisany dziś (NVS `sp_model` = dziś, ustawia wołający): pole `Cofnij nowy model z dziś`
  bool nowyModelDzis = false;
  bool zrodloOto = false;        // panel ma źródło otoczenia (OTO_URL): tylko wtedy jest pole CO2
};
// Pola z `/konfiguracja.txt` i sekretów (stan przed formularzem). `zrodloOto` = panel ma lokalne źródło otoczenia.
// `wymiana.filtr` w innej postaci niż RRRR-MM-DD = pole puste; `okres.*` dłuższe niż MAKS_OKRESU_Z_PLIKU bajtów
// przycięte (taka wartość i tak jest zła: poprawna ma najwyżej 3 cyfry).
// `szerSecrets` i `dlSecrets` = `POGODA_LAT` i `POGODA_LON` z secrets.h (NAN = brak): bez `miasto=` w pliku formularz
// zaznacza miasto z listy, jeśli te współrzędne leżą w granicy 0,05° od niego, inaczej „nie wybrano”.
Pola zPliku(const std::string& konfiguracja, const sekrety::Dane& s, bool zrodloOto = false, double szerSecrets = NAN,
            double dlSecrets = NAN);

// Ucieczka znaków HTML (treść i wartości atrybutów): < > & " ' (XSS przez nazwę sieci, R-KONF-38).
std::string html(const std::string& s);
// Ciało `application/x-www-form-urlencoded` → pary (dekodowanie `+` i `%XX`; zły `%` zostaje dosłownie). Ostatni
// wygrywa. Najwyżej MAKS_CIALA bajtów (więcej = puste); ta sama granica co w adapterze (serwer::MAKS_CIALA_POST).
const size_t MAKS_CIALA = serwer::MAKS_CIALA_POST;
std::map<std::string, std::string> parsujCialo(const std::string& cialo);
// Kodowanie odwrotne (adapter ESP32 składa ciało z argumentów WebServer, testy).
std::string koduj(const std::string& s);

// Wynik walidacji formularza (R-KONF-18: zła wartość odrzucona, poprzednia zostaje, reszta poprawnych pól zapisana).
struct Wynik {
  std::map<std::string, std::string> bledy;  // nazwa pola → opis błędu przy polu
  // Zmiany pliku: klucz → nowa wartość; "" = usuń wiersz (wartość domyślna).
  std::map<std::string, std::string> plik;
  sekrety::Dane sekrety;  // nowe sekrety (z hasłami zapisanymi, gdy pole puste)
  bool zmianaSieci = false;      // inna nazwa albo hasło sieci domowej
  bool zmianaAirBridge = false;  // inny adres, konto albo hasło AirBridge
  // sekcja „Aktualizacje”: nowy token ("" = bez zmiany), usunięcie tokenu, nowe hasło okna ("" = bez zmiany)
  std::string kluczWydan;
  bool usunKlucz = false;
  std::string hasloOkna;
  // R-MAS-16: pole `Nowy model maski od dziś` zaznaczone (data wymiany maski = dziś już w `plik`); wołający zapisuje
  // datę nowego modelu (zerowanie normy M-07 i linia R-MAS-16, dawniej R-MAS-11) po zapisie pliku
  bool nowyModelMaski = false;
  // R-MAS-19: pole `Cofnij nowy model z dziś` zaznaczone; wołający przywraca datę maski i `sp_model` sprzed dziś
  bool cofnijNowyModel = false;
  // R-DS-32, R-DS-33: przy ponownym włączeniu wybrano `od nowa`; wołający zachowuje kopię stanu i zaczyna od nowa
  bool startOdNowa = false;
  bool ok() const { return bledy.empty(); }
};
// `dzis` RRRR-MM-DD: daty wymiany z przyszłości odrzucone (R-KONF-34). `obecne` = sekrety przed formularzem.
Wynik waliduj(const std::map<std::string, std::string>& pola, const sekrety::Dane& obecne, const std::string& dzis);
// Sekcja „Ekrany” (ADR-0045, R-KONF-60; woła ją waliduj()): pola `ekrany_start`, `kk_start`, `dk_start`, `kk_{id}`,
// `dk_{id}`, `pokaz_{id}` → w.plik[`kokpit.kolejnosc`, `kokpit.ukryte`, `dzien.kolejnosc`, `dzien.ukryte`] ("" = domyślne,
// wiersz usuwany). Bez `ekrany_start` nic; zła wartość miejsca → błąd przy polu, układ bez zmian (R-KONF-18).
// R-KON-77: pole `wygaszacz` (`tak` | `nie`) → `dzien.wygaszacz` (`tak` = wiersz usuwany; bez pola, np. strona sprzed
// zmiany: bez zmian); zła wartość → błąd przy polu, sekcja bez zmian. Pole `wygaszacz_min` → `dzien.wygaszacz_min`
// (180 = wiersz usuwany; puste = domyślne; liczba spoza 30–720 → błąd przy polu, poprzednia wartość zostaje; przy
// `wygaszacz=nie` pole pominięte).
void walidujEkrany(const std::map<std::string, std::string>& pola, Wynik& w);
// Adres AirBridge: IPv4 z sieci prywatnej albo nazwa `.local`, opcjonalnie `:{port}` 1–65535 (5.6.4, R-KONF-40).
bool adresAirBridgeOk(const std::string& a);

// Nowa treść `/konfiguracja.txt`: wiersze z kluczami z `zmiany` zastąpione (pierwszy w miejscu, kolejne usunięte),
// nowe dopisane na końcu, wartość "" usuwa wiersze klucza; komentarze i inne klucze bez zmian.
std::string scal(const std::string& plik, const std::map<std::string, std::string>& zmiany);

// Strona formularza. `akcja` = ścieżka POST (`/u/{token}/`); `komunikat` nad formularzem (np. `Zapisano o 21:14.`,
// wynik sprawdzenia AirBridge); `bledy` przy polach. Bez zewnętrznych zasobów (bez http/https w atrybutach).
// Układ z 14.11.2026 (R-KONF-59): sekcje zwijane `<details>` z bieżącą wartością w nagłówku (nigdy hasło ani klucz),
// sekcja z błędem otwarta (także „Dźwięk i humor” przy wyłączonym dźwięku), przycisk `Zapisz na panelu` przyklejony
// do dołu, licznik niezapisanych zmian z małego skryptu w stronie (bez skryptu ukryty; po odesłaniu z błędami ukryty do
// pierwszej zmiany pola; nazwy pól, walidacja i zapis bez zmian), wersja w nagłówku. Składana w jednym `std::string`
// z rezerwacją REZERWACJA_STRONY bajtów: najgorszy realny przypadek (najdłuższe sekrety z samych znaków ucieczki,
// prawdziwe błędy wszystkich pól, najdłuższy komunikat dyrygenta) mieści się w niej bez realokacji.
// od 16.11.2026 (limit ładowania i wygaszacz razem) 23 KB: scenariusz brzegowy (błąd 91 B przy każdym
// polu, także przy `ladowanie_limit`, `wygaszacz`, `wygaszacz_min`, czas 720) ma 21 749 B, najgorszy realny przypadek
// 19 574 B (zmierzone w test/testy_formularz_u03.cpp); 23 KB daje zapas co najmniej 1 KB także w scenariuszu
// brzegowym (1 803 B), jak dla przypadku realnego. Pole `ladowanie_limit` (ADR-0046) dokłada w tym scenariuszu
// 535 B, pola wygaszacza (R-KON-74, R-KON-77) 681 B. Wcześniej: 22 KB (ADR-0046) albo 21 KB (wygaszacz) na osobnych
// gałęziach, od ADR-0045 (sekcja „Ekrany”) 20 KB, przedtem 16 KB
// Od sekcji „Dobry start” (ADR-0049, 22.11.2026) 26 KB: pięć pól z tekstami ze spec 5.6.6 i błędy przy nich dokładają
// ok. 2,4 KB w scenariuszu brzegowym (test/testy_formularz_u03.cpp podaje zmierzony rozmiar i zapas).
// Pole kanału wczesnego (R-KONF-62, 23.11.2026) i „Dobry start” wersji 2 (sekcja bez pola „ponad program”) bez zmiany
// rezerwacji; aktualny rozmiar i zapas podaje test/testy_formularz_u03.cpp (23.11.2026: scenariusz brzegowy
// 24 467 B, zapas 2 157 B; najgorszy realny 21 780 B, zapas 4 844 B).
const size_t REZERWACJA_STRONY = size_t{26} * 1024;
// Najdłuższa wartość `okres.maska` / `okres.filtr_tyg` z pliku przenoszona do pola formularza (bajty).
const size_t MAKS_OKRESU_Z_PLIKU = 8;
std::string strona(const Pola& p, const std::string& akcja, const std::string& komunikat,
                   const std::map<std::string, std::string>& bledy);

// ---------- program „Dobry start”: strony pod tym samym adresem formularza (R-DS-25, R-DS-33, R-DS-35, 8.2, 8.3) ----------
// `/u/{token}/ankieta` (GET strona, POST zapis), `/u/{token}/porady?t={n}` (GET), `/u/{token}/odnowa` (GET
// potwierdzenie, POST `tak=1`). Strony bez zewnętrznych zasobów i bez skryptu; nic nie wychodzi poza panel (ADR-0001).
const size_t MAKS_STRONY_PORAD = size_t{8} * 1024;  // R-DS-35 [PRÓG]: jedna strona „Wszystkie porady”
// Wartość klucza `start.*` do zapisu w `/konfiguracja.txt` (formularz i pytanie startowe ankiety, ta sama konwencja):
// wartość domyślna pola (np. `start.nawilzacz=nie_wiem`) = "" (wiersz usunięty, R-KONF-20), inna bez zmian.
std::string wartoscDoPliku(const std::string& klucz, const std::string& v);
std::string stronaAnkiety(const dobry_start::Katalog& k, const dobry_start::Stan& s, const dobry_start::Ustawienia& u,
                          const std::string& dzis, const std::string& akcja, const std::string& komunikat);
// Odpowiedzi z ciała POST: pole `q_{id}` (jedna odpowiedź, numer od 1) albo `q_{id}_{k}` (wiele).
std::vector<std::pair<std::string, std::vector<int>>> odpowiedziAnkiety(const dobry_start::Katalog& k,
                                                                        const std::map<std::string, std::string>& m);
// Zakładka `t` 1…zakladekPorad(k) (wersja 2): `Start, rano`, `Start, wieczór`, tygodnie adaptacji, utrwalenie,
// `Pochwały`, `Po nocy`, `Z ankiety`; `podstaw` = wartości znaczników `{prog:id}` (R-DS-53). Zła zakładka = pierwsza.
int zakladekPorad(const dobry_start::Katalog& k);
std::string stronaPorad(const dobry_start::Katalog& k, const dobry_start::Stan& s, const dobry_start::Ustawienia& u,
                        dobry_start::Tryb tryb, int t, const std::string& akcja,
                        std::string (*podstaw)(const std::string&, void*), void* kontekst);
std::string stronaOdNowa(const std::string& akcja);
extern const char* const TEKST_ANKIETA_ZAPISANA;  // `Zapisano. Porady do twoich odpowiedzi pojawią się …`
extern const char* const TEKST_ANKIETA_NIEZAPISANA;  // zapis stanu programu albo ustawień nieudany

// Odpowiedź sesji formularza: strona albo zapis do wykonania przez wołającego (dyrygent).
struct Odpowiedz {
  int kod = 404;
  std::string typ = "text/plain; charset=utf-8";
  std::string cialo = "Nie ma takiej strony.";
  bool zapis = false;                          // POST z ważnym tokenem: wołający waliduje `pola`, zapisuje, buduje stronę
  std::map<std::string, std::string> pola;     // ciało POST
  // ADR-0049: podstrona programu „Dobry start” (`ankieta`, `porady`, `odnowa`; "" = formularz) i zakładka `t`
  std::string podstrona;
  int zakladka = 1;
};

// Sesja serwera formularza: token 128 bitów, 15 min, 404 bez ważnego tokenu, wyłączenie po 5 złych żądaniach
// (R-KONF-13, R-KONF-14, R-KONF-15). Ścieżka `/u/{token}/` (GET strona, POST zapis).
class Sesja {
 public:
  void otworz(const std::string& token, uint32_t ms, const std::string& adres);
  bool aktywna(uint32_t ms) const;
  void zamknij() { otwarta_ = false; }
  bool zamknietaZlymi() const { return zle_ >= serwer::ZLE_MAKS; }
  uint32_t koniecMs() const { return od_ + serwer::CZAS_MS; }
  std::string sciezka() const { return "/u/" + token_ + "/"; }
  std::string adresQr() const { return "http://" + adres_ + sciezka(); }
  std::string adresReczny() const { return adres_ + "/u/" + token_; }
  // `metoda` "GET" albo "POST"; `strona` = bieżąca strona formularza (GET), przenoszona do odpowiedzi bez kopii
  // (strona ma do 20 KB rezerwacji, druga kopia podwajała szczyt sterty przy podawaniu strony).
  Odpowiedz obsluz(const std::string& metoda, const std::string& sciezka, const std::string& cialo, uint32_t ms,
                   std::string strona);
  int zleZadania() const { return zle_; }

 private:
  bool otwarta_ = false;
  uint32_t od_ = 0;
  int zle_ = 0;
  std::string token_, adres_;
};

}  // namespace formularz
