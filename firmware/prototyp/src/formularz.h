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

#include "sekrety.h"
#include "serwer_raportu.h"

namespace formularz {

const int ELEMENTY = 2;  // maska, filtr (5.9.2; klucze `wymiana.*`, `okres.maska`, `okres.filtr_tyg`)
extern const char* const KLUCZE_ELEMENTOW[ELEMENTY];  // "maska", "filtr"
extern const char* const NAZWY_ELEMENTOW[ELEMENTY];   // "Maska", "Filtr"

// Wartości do pokazania w formularzu (obecny stan panelu), bez haseł.
struct Pola {
  std::string ssid;
  bool hasloWifi = false;  // hasło zapisane: pole puste z napisem `ustawione`
  std::string abAdres, abKonto;
  bool hasloAb = false;
  std::string profil = "prywatny";   // `publiczny` | `prywatny`
  std::string wymiana[ELEMENTY];     // RRRR-MM-DD albo ""
  std::string okres[ELEMENTY];       // maska: miesiące, filtr: tygodnie, "" = domyślny okres
  // (5.6.4). Wartości domyślne ze spec, gdy w pliku nie ma wiersza.
  bool zrodloOto = false;             // panel ma źródło otoczenia (OTO_URL): tylko wtedy jest pole CO2
  std::string co2 = "tak";            // `tak` | `nie`
  std::string humor = "normalny";     // `wylaczony` | `oszczedny` | `normalny`
  std::string buzzer = "tak";         // `tak` | `nie`
  std::string ciszaOd = "22:00", ciszaDo = "08:00";  // GG:MM
  std::string podpowiedz = "tak";     // `tak` | `nie`
  std::string miasto;                 // "" = nie wybrano, albo nazwa z listy miast (src/miasta.h)
  std::string czynniki = "KL";        // kody zaznaczonych czynników raportu (bez wiersza: stan domyślny 5.13.7)
  std::string uwagaOMasce = "10";     // R-KONF-42 „Uwaga o masce od”, L/min 10–15 (klucz `uwaga_o_masce`)
  // sekcja „Aktualizacje” (ADR-0038): automat z pliku (`aktualizacja_auto=tak`,
  // domyślnie `nie`); token wydań i hasło okna serwisowego tylko jako „ustawiony” (wartości nigdy do przeglądarki)
  std::string aktualizacjaAuto = "nie";  // `tak` | `nie`
  bool kluczWydan = false;               // token wydań zapisany w pamięci panelu (wołający ustawia z NVS)
  bool hasloOkna = false;                // hasło okna serwisowego w pamięci panelu (wołający ustawia z NVS)
};
// Pola z `/konfiguracja.txt` i sekretów (stan przed formularzem). `zrodloOto` = panel ma lokalne źródło otoczenia.
// `szerSecrets` i `dlSecrets` = `POGODA_LAT` i `POGODA_LON` z secrets.h (NAN = brak): bez `miasto=` w pliku formularz
// zaznacza miasto z listy, jeśli te współrzędne leżą w granicy 0,05° od niego, inaczej „nie wybrano”.
Pola zPliku(const std::string& konfiguracja, const sekrety::Dane& s, bool zrodloOto = false, double szerSecrets = NAN,
            double dlSecrets = NAN);

// Ucieczka znaków HTML (treść i wartości atrybutów): < > & " ' (XSS przez nazwę sieci, R-KONF-38).
std::string html(const std::string& s);
// Ciało `application/x-www-form-urlencoded` → pary (dekodowanie `+` i `%XX`; zły `%` zostaje dosłownie). Ostatni
// wygrywa. Najwyżej MAKS_CIALA bajtów (więcej = puste).
const size_t MAKS_CIALA = 4096;
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
  bool ok() const { return bledy.empty(); }
};
// `dzis` RRRR-MM-DD: daty wymiany z przyszłości odrzucone (R-KONF-34). `obecne` = sekrety przed formularzem.
Wynik waliduj(const std::map<std::string, std::string>& pola, const sekrety::Dane& obecne, const std::string& dzis);
// Adres AirBridge: IPv4 z sieci prywatnej albo nazwa `.local`, opcjonalnie `:{port}` 1–65535 (5.6.4, R-KONF-40).
bool adresAirBridgeOk(const std::string& a);

// Nowa treść `/konfiguracja.txt`: wiersze z kluczami z `zmiany` zastąpione (pierwszy w miejscu, kolejne usunięte),
// nowe dopisane na końcu, wartość "" usuwa wiersze klucza; komentarze i inne klucze bez zmian.
std::string scal(const std::string& plik, const std::map<std::string, std::string>& zmiany);

// Strona formularza. `akcja` = ścieżka POST (`/u/{token}/`); `komunikat` nad formularzem (np. `Zapisano o 21:14.`,
// wynik sprawdzenia AirBridge); `bledy` przy polach. Bez zewnętrznych zasobów (bez http/https w atrybutach).
std::string strona(const Pola& p, const std::string& akcja, const std::string& komunikat,
                   const std::map<std::string, std::string>& bledy);

// Odpowiedź sesji formularza: strona albo zapis do wykonania przez wołającego (dyrygent).
struct Odpowiedz {
  int kod = 404;
  std::string typ = "text/plain; charset=utf-8";
  std::string cialo = "Nie ma takiej strony.";
  bool zapis = false;                          // POST z ważnym tokenem: wołający waliduje `pola`, zapisuje, buduje stronę
  std::map<std::string, std::string> pola;     // ciało POST
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
  // `metoda` "GET" albo "POST"; `strona` = bieżąca strona formularza (GET).
  Odpowiedz obsluz(const std::string& metoda, const std::string& sciezka, const std::string& cialo, uint32_t ms,
                   const std::string& strona);
  int zleZadania() const { return zle_; }

 private:
  bool otwarta_ = false;
  uint32_t od_ = 0;
  int zle_ = 0;
  std::string token_, adres_;
};

}  // namespace formularz
