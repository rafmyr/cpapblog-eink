// SPDX-License-Identifier: GPL-3.0-or-later
// Sprawy z wyników silnika reguł: cienka warstwa nad
// `std::vector<reguly::Wynik>` (kontrakt src/reguly.h) do ekranu wzorca L, „Otwartych spraw” (5.4.15), kolejki linii
// uwagi (src/kolejka.h), buzzera pilności A (5.14) i pytań tak/nie (5.12.8). Bez zależności od Arduino.
//
// Do scalenia (src/wzorce.h: SprawaL, sprawyOtwarte, wiem, omowione) ta warstwa sama czyta stan wzorca z
// wyniku i z pamięci reguł: spełniony wynik = sprawa otwarta, `wiem` i `omowione` z PamiecReguly.
// Logiki stanów wzorca 5.12.12 (wygaszony B, wstrzymany, karencja, powrót po Δ) tu nie ma: robi ją src/wzorce.cpp; po scaleniu
// sprawyOtwarte() bierze stan z wzorce.h, a reszta tego pliku zostaje.
//
// Teksty L i E biorą się wyłącznie z zestawu (reguly::tekst) przez FunkcjaTekstu, żeby ten plik nie wpisywał
// tekstów reguł ręcznie i dał się testować przed (w testach atrapa zestawu). Wzory linii ogólnych (`Wciąż: {tytuł}.`
// i inne z 7.5) są w src/kolejka.cpp z odwołaniem do spec.
#pragma once
#include <functional>
#include <string>
#include <vector>

#include "noc.h"
#include "reguly.h"
#include "reguly_sygnaly.h"
#include "stany.h"

namespace sprawy {

// Tekst reguły po podstawieniu zmiennych: na panelu reguly::tekst(zestaw, id, wynik, tryb); w testach atrapa.
using FunkcjaTekstu = std::function<std::string(const std::string& idTekstu, const reguly::Wynik& w)>;

// Role pliku tekstów (spec techniczna 9.3) i role podpisów wizualizacji ekranu L (5.12.5 „Podpisy osi wizualizacji”):
// `podpis_lewo`, `podpis_prawo`, a dla L-08 nazwy rzędów `rzad_1`, `rzad_2` (do dopisania w formacie tekstów).
enum class Rola : unsigned char { TYTUL, POWOD, KROK, LINIA, PODPIS_LEWO, PODPIS_PRAWO, RZAD_1, RZAD_2 };

// Jak sprawa trafia na ekran: ton „lekarz” (L z pilnością A albo B, grupy R2 i R3), ton „problem” (L-09, grupa R5c:
// S3 z krokiem do dostawcy), bez pełnego ekranu (pilność C, „tylko raport”, zdarzenia E: ich ekrany to src/ekrany_zdarzen.*).
enum class Ton : unsigned char { BRAK, LEKARZ, PROBLEM };

struct SprawaL {
  std::string klucz;       // klucz pamięci reguł: "id" albo "id/wariant"
  std::string id, wariant;
  reguly::StanReguly stan = reguly::StanReguly::AKTYWNA;
  reguly::Pilnosc pilnosc = reguly::Pilnosc::BRAK;
  std::string grupa;       // z katalogu: R2, R3, R5b, R5c …
  Ton ton = Ton::BRAK;
  std::string tytul, powod, krok, linia;  // z zestawu (rola z pliku tekstów, spec techniczna 9.3)
  std::string podpisLewo, podpisPrawo, rzad1, rzad2;  // podpisy wizualizacji z zestawu
  double progLinii = NAN;  // linia przerywana wizualizacji: próg z parametru `prog_linii` albo `prog` wiersza katalogu
  std::string hipoteza;    // linia ramki hipotezy H (5.12.9): wypełnia src/wzorce.cpp; "" = bez ramki
  std::string aktywnaOd;   // data poranka wejścia (PamiecReguly::aktywnaOd; "" = wejście dziś)
  std::string wiem, omowione;  // data poranka naciśnięcia; "" = nie było
  std::string pominiete, okno;  // ze zmiennych wyniku: noce pominięte (duży wyciek) i okno nocy, do wiersza `Ocena:`
  // stan wzorca 5.12.12 z pamięci (wzorce::nazwaStanu: "aktywny", "wiem", "omowione", "wygaszony_b",
  // "wstrzymany", "karencja"; "" = brak zapisu) i data ostatniego rzeczywistego pokazania pełnego ekranu (R-PIE-08)
  std::string stanWzorca, pokazanoPelny;
  std::string kluczWzorca;          // klucz pamięci wzorca (wzorce::kluczWzorca; "" = nie L)
  bool pelnyMimoOmowienia = false;  // A po trzecim „Omówione” bez poprawy: dziś dzień pełnego ekranu (R-LEK-27)
  // zdarzenia E z poziomem „problem” (S3, grupy R5b–R5d, 6.4) i problem poranny L-09 (R5c)
  std::string pokazanoPoprzednio;  // przedostatnie rzeczywiste pokazanie pełnego ekranu (R-KON-14: 2 razy w 7 dniach)
  bool wykluczona = false;         // zdarzenie wykluczone przez inne (7.2, `wykluczona_przez`): karta bez ekranu i linii
  std::string metrykaWiz, metrykaWizCpap;  // metryka wykresu S3 z parametru `metryka` katalogu: w trybie ASV i ASVAuto
                                           // i w CPAP i AutoSet (id progu z metryką trybu, E-36.sygnal_cisnienia, 4.11)
  bool jestE() const { return id.size() > 2 && id.compare(0, 2, "E-") == 0; }
  bool wygaszonyB() const { return stanWzorca == "wygaszony_b"; }
  bool wstrzymany() const { return stanWzorca == "wstrzymany"; }
  bool karencja() const { return stanWzorca == "karencja"; }
  bool jestL() const { return id.size() > 2 && id.compare(0, 2, "L-") == 0; }
  bool tylkoRaport() const { return stan == reguly::StanReguly::TYLKO_RAPORT; }
};

std::string klucz(const reguly::Wynik& w);
const reguly::WierszKatalogu* wierszKatalogu(const reguly::Zestaw& z, const reguly::Wynik& w);
// Tekst roli z listy tekstów wiersza katalogu (pierwszy id_tekstu o tej roli); "" gdy go nie ma.
std::string tekstRoli(const reguly::Zestaw& z, const reguly::WierszKatalogu& k, const reguly::Wynik& w, Rola r,
                      const FunkcjaTekstu& f);

// Sprawy otwarte (R-EKR-48 (1), (2), (4) w zakresie wyników): każdy wynik oceniony i spełniony, poza regułą
// ZABLOKOWANA; L, zdarzenia R5b i L-09. Kolejność R-EKR-48: pilność A, pilność B, zdarzenia R5b, pilność C, reszta L
// (w tym „tylko raport”), L-09; w grupie kolejność katalogu (od najstarszej: aktywnaOd rosnąco).
std::vector<SprawaL> sprawyOtwarte(const reguly::Zestaw& z, const std::vector<reguly::Wynik>& wyniki,
                                   const reguly::Pamiec& pamiec, const FunkcjaTekstu& f);

// uzupełnia sprawy L o dzień pełnego ekranu A po trzecim „Omówione” bez poprawy (R-LEK-27, wzorce::info).
// Wołać po sprawyOtwarte() z tymi samymi wynikami i pamięcią.
void stanyWzorcow(std::vector<SprawaL>& s, const std::vector<reguly::Wynik>& wyniki, const reguly::Pamiec& pamiec,
                  const std::string& dzis, const reguly::Zestaw* z);

// „Wiem” (5.12.7, R-PRZ-03): pilność A na 3 dni, B (i L-09, 5.6.2) na 7 dni [PRÓG]; dzień naciśnięcia = dzień 1.
// przy zapisanym stanie wzorca wyciszenie trwa tylko w stanie „wiem” („omowione”) albo w dniu naciśnięcia
// (stan liczy się raz na dobę), więc powrót przed czasem (Δ, wzrost pilności), 28 nocy po drugim „Omówione”
// (R-LEK-25) i koniec przy zmianie ustawień (R-LEK-09) idą za stanem wzorca 5.12.12.
int dniWiem(const SprawaL& s);
bool wiemAktywne(const SprawaL& s, const std::string& dzis);
// „Omówione z lekarzem” (5.12.7): 56 nocy [PRÓG] liczone jako 56 dób od dnia potwierdzenia (bez „do zmiany ustawień”).
const int NOCY_OMOWIONE = 56;
bool omowioneAktywne(const SprawaL& s, const std::string& dzis);
// Dni od wejścia (0 = dzień wejścia); bez daty wejścia 0.
int dniOdWejscia(const SprawaL& s, const std::string& dzis);

// Pełny ekran wzorca dziś (5.12.2, 7.4): tylko AKTYWNA z tonem; A w dniu wejścia i co 3 dni [PRÓG], B i L-09 w dniu
// wejścia i co 7 dni [PRÓG]; nigdy w czasie „wiem”; A po „Omówione” bez pełnego ekranu (linia R-LEK-24), B po
// „Omówione” cisza. (R-PIE-08): rytm liczony od ostatniego rzeczywistego pokazania (`pokazanoPelny`), więc dzień
// przegrany z S5 albo z inną sprawą nie zużywa „co 3 dni” i ekran wraca następnego dnia; bez pokazania od wejścia
// ekran należy się od razu, a w dniu pokazania przez cały dzień. Wygaszony B (R-SKI-09), wstrzymany (R-INF-02)
// i karencja (R-SKI-08) bez pełnego ekranu.
bool pelnyEkranDzis(const SprawaL& s, const std::string& dzis);
// R-KON-14 (7.4 „ten sam problem poranny”): ekran problemu porannego (zdarzenie E z grup R5b–R5d i L-09) pokazany
// 2 razy w ostatnich 7 dniach [PRÓG] nie wraca na pole główne do upływu 7 dni od pierwszego z tych pokazań; w dniu
// pokazania ekran stoi cały dzień. Liczą się tylko rzeczywiste pokazania (R-PIE-08: `pokazanoPelny`,
// `pokazanoPoprzednio`; „2 razy” = te dwie daty, więc bez osobnej stałej). Przykład spec: E-21 aktywne 10 dni → ekran
// w dniach 1, 2, 8 i 9.
const int LIMIT_PROBLEMU_DNI = 7;
bool limitProblemu(const SprawaL& s, const std::string& dzis);
// Problem poranny (S3): zdarzenie E z poziomem „problem” albo L-09 (ton PROBLEM).
bool problemPoranny(const SprawaL& s);
// Miejsce w tabeli 7.2 problemów porannych: R5b (E-30 > E-31 > E-33 > E-36 > E-37 > S-K-17 > E-23) przed R5c (S-K-31 >
// L-09 > E-22 > E-21 > S-K-14 > S-K-11 > S-K-10) przed R5d (E-10 > E-12 > E-13 > E-14 > S-K-16); mniejsza = wyżej.
int rangaProblemu(const SprawaL& s);

// ---------- linie zdarzeń E ----------
// Zdarzenie E z poziomem `linia` (E-04, E-05, E-07, E-10/linia, E-11, E-20, E-21/wilgotność): linia uwagi w grupie
// z katalogu (`noc`, `dane`, `awarie`). Bez E-08 (linię E-08 daje dyrygent ze stanu źródła otoczenia, raz na epizod).
struct LiniaZdarzenia {
  std::string id;     // klucz wyniku ("E-10/linia")
  std::string grupa;  // „noc”, „dane”, „awarie”
  std::string tekst;
};
std::vector<LiniaZdarzenia> linieZdarzen(const reguly::Zestaw& z, const std::vector<reguly::Wynik>& wyniki,
                                         const FunkcjaTekstu& f);

// ---------- pokazania (R-PIE-08: limity liczą tylko to, co rzeczywiście stało na ekranie) ----------
// Wpis pomocniczy w pamięci reguł `pokaz~{co}` (PamiecReguly: `aktywnaOd` = pierwsze, `ostatnio` = ostatnie pokazanie,
// `licznik` = dni z pokazaniem); `co` = klucz sprawy (pełny ekran L) albo `linia:{nazwa}` (linia uwagi z rytmem:
// R-NOR-21, R-INF-10, R-SKI-41, R-PIE-09). Format pliku `reguly` bez zmian (reguly.h): wpis ma postać wiersza
// reguły o id zaczynającym się od `pokaz~`, którego silnik nie zna i nie zmienia (aktualizujPamiec() bierze tylko
// klucze z wyników). Zwraca true, gdy pamięć się zmieniła (wtedy zapis pliku).
std::string kluczPokazania(const std::string& co);
bool kluczPomocniczy(const std::string& kluczPamieci);  // `pokaz~…`
bool zapiszPokazanie(reguly::Pamiec& p, const std::string& co, const std::string& dzis);
std::string ostatniePokazanie(const reguly::Pamiec& p, const std::string& co);  // "" = nigdy
// Sprawa, która dziś jest ekranem dnia: najwyższa pilność (A przed B, potem L-09 z grupy R5c), w grupie pierwsza
// z listy; nullptr = żadna.
const SprawaL* sprawaEkranuDnia(const std::vector<SprawaL>& s, const std::string& dzis);

// Zapis „wiem” i „Omówione” w pamięci reguł (pola PamiecReguly::wiem, omowione). Pole ma postać
// `RRRR-MM-DD@P`: data poranka i pilność z chwili potwierdzenia (`A`, `B`, `C`, `-`), bo wzrost pilności kończy
// wyciszenie przed czasem (5.12.7 „Powrót przed czasem”, R-EKR-23, R-LEK-10). Samo `RRRR-MM-DD` (bez `@`) = pilność
// nieznana, wyciszenie trwa. Osobnego pola w PamiecReguly nie ma (reguly.h bez zmian).
void zapiszWiem(reguly::Pamiec& p, const std::string& klucz, const std::string& dzis,
                reguly::Pilnosc pil = reguly::Pilnosc::BRAK);
void zapiszOmowione(reguly::Pamiec& p, const std::string& klucz, const std::string& dzis,
                    reguly::Pilnosc pil = reguly::Pilnosc::BRAK);
// Data z pola wyciszenia (`RRRR-MM-DD@P` albo `RRRR-MM-DD`) i czy pilność od potwierdzenia wzrosła.
std::string dataWyciszenia(const std::string& pole);
bool pilnoscWzrosla(const std::string& pole, reguly::Pilnosc teraz);
// Cofnięcie „wiem” (ekran „Przeniesione”, R-PRZ-16).
void cofnijWiem(reguly::Pamiec& p, const std::string& klucz);

// ---------- metryka zdrowotna w stanie odchylenia (R-EKR-48 (3)) ----------
struct SprawaMetryki {
  std::string nazwa;  // `AHI`, `EPAP w szczycie`
  int nocy = 0;       // noce trwania stanu odchylenia
  std::string x, p10, p90;  // mediana 5 ostatnich nocy ważnych i pasmo normy, już sformatowane
  std::string x7;           // mediana 7 ostatnich nocy ważnych (R-NOR-21), sformatowana
  std::string id;           // metryka: "M-02" (AHI), "M-06p" (EPAP P95)
  std::string od;           // data poranka pierwszej nocy stanu (R-NOR-21 `{n}` = pełne tygodnie od niej)
};
// Z metryk zdrowotnych, których stan liczy panel (stany.h): AHI (M-02) i EPAP P95 (M-06p, tryb ASV).
std::vector<SprawaMetryki> sprawyMetryk(const std::vector<Noc>& hist, const Stany& st, const std::string& dzis);

// ---------- „Otwarte sprawy” (5.4.15, R-EKR-47…R-EKR-50) ----------
struct KartaSprawy {
  std::string tytul, powod;
  std::string ocena, ty, dalej, koniec;  // wartości wierszy bez nazw (`Ocena:` …)
  std::string dopisek;  // stan pusty przy ocenie częściowej, wiersz pod `Ocena:` (R-EKR-50)
};
struct WejscieSpraw {
  std::string dzis;
  const std::vector<SprawaL>* sprawy = nullptr;
  const std::vector<SprawaMetryki>* metryki = nullptr;
  std::vector<std::string> linieZdrowia;  // linie grupy „zdrowie” bez sprawy z kartą (R-EKR-48 (5)), np. senność
  const reguly::Odpowiedzi* odpowiedzi = nullptr;  // `Ty: gorzej: tak, {data}`
  const SprawaL* ekranDnia = nullptr;     // sprawa, która dziś jest ekranem dnia (`Dalej: na ekranie dnia`)
  bool ostatniaNiepewna = false;          // stan pusty: `Ocena: ograniczona: ostatnia noc z dużym wyciekiem`
  // (ADR-0029 pkt 7): stan pusty mówi `Ocena: pełna` tylko wtedy, gdy wołający wie, że każda reguła
  // jest oceniona (5.4.13 pkt 7) i lista `Metryki niewalidowane` jest policzona i pusta (pkt 5). Panel pkt 5 jeszcze
  // nie liczy (WY9), więc dziś nikt tego nie ustawia. bez pewności
  // `Ocena: częściowa` i wiersz `Części rzeczy jeszcze nie oceniam.` (tytuł wtedy `Nie widzę otwartych spraw.`) (R-EKR-50).
  bool ocenaPelna = false;
};
// Karty w kolejności R-EKR-48; pusta lista = stan pusty (R-EKR-50, kartaPusta()).
std::vector<KartaSprawy> kartySpraw(const WejscieSpraw& w);
KartaSprawy kartaPusta(const WejscieSpraw& w);

// ---------- odpowiedzi tak/nie (5.12.8) ----------
// Plik `odpowiedzi` (rejestr src/magazyn.cpp): `/dane/odpowiedzi.csv`, pola `data;pytanie;odpowiedz`, pytanie
// `sennosc`, `gorzej`, `przerwa`, odpowiedź `tak`, `nie`, `brak` (brak odpowiedzi to nie „nie”, R-SKI-17).
// INFEKCJA = `Katar albo infekcja?` na S-K-30 (5.10.2), PODUSZKA = `Wymieniona poduszka?` (R-MAS-12)
// WYJAZD: `Spanie poza domem od {data}?` (R-CZY-22); data odpowiedzi = data poranka pierwszej nocy okresu.
enum class Pytanie : unsigned char { SENNOSC, GORZEJ, PRZERWA, WYJAZD, INFEKCJA, PODUSZKA };
const char* nazwaPytania(Pytanie p);
struct Odpowiedz {
  std::string data;  // data poranka (senność: niedziela tygodnia)
  Pytanie pytanie = Pytanie::SENNOSC;
  char odp = 'b';    // 't', 'n', 'b' (brak odpowiedzi)
};
bool parsujOdpowiedz(const std::string& wiersz, Odpowiedz& o);
std::string wierszOdpowiedzi(const Odpowiedz& o);
// Jedna odpowiedź na pytanie i datę: nowa zastępuje starą (cofnięcie i ponowna odpowiedź, R-PRZ-11).
void zapiszOdpowiedz(std::vector<Odpowiedz>& lista, const Odpowiedz& o);
void cofnijOdpowiedz(std::vector<Odpowiedz>& lista, const std::string& data, Pytanie p);
bool jestOdpowiedz(const std::vector<Odpowiedz>& lista, const std::string& data, Pytanie p);
// reguly::Odpowiedzi dla silnika: tylko „tak” i „nie” (brak odpowiedzi nie trafia do map), przerwa: daty z „tak”.
reguly::Odpowiedzi odpowiedziSilnika(const std::vector<Odpowiedz>& lista);

// Zastępcze źródło wyników dla testów dyrygenta i symulatora do scalenia (src/dyrygent.cpp ocenSilnikiem):
// gdy `wyniki` nie jest nullptr, dyrygent bierze wyniki z niego, a teksty z `zestaw` przez `tekst`. Panel go nie ustawia.
struct ZrodloWynikow {
  std::vector<reguly::Wynik> (*wyniki)(const reguly::Kontekst& k) = nullptr;
  const reguly::Zestaw* zestaw = nullptr;
  FunkcjaTekstu tekst;
  void (*sygnaly)(const reguly::Sygnaly& s) = nullptr;  // podgląd sygnałów komponentów podanych do oceny (testy)
};
extern ZrodloWynikow zrodloTestowe;

}  // namespace sprawy
