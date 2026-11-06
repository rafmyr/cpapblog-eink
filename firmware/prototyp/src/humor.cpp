// SPDX-License-Identifier: GPL-3.0-or-later
#include "humor.h"
#include "kokpit.h"
#include "kontekst_sk.h"
#include "wspolne.h"
#include <algorithm>
#include <random>
#include <cstdio>
#include <cstdlib>

// Pule ze spec 5.7.5, wszystkie typy ekranów z kartą; kolejność zdań = numery w `/humor.csv`.
static const Pula PULE[] = {
    {"dobra_noc",
     {"Kawa dla mnie, wyniki dla ciebie.", "Nic nie dzwoniło. Poza mną.", "Przespane. Ja tylko tykałem."},
     {"Aparat pracował, a medal biorę ja.", "Tak cicho, że słyszałem własne tykanie.", "Szukałem dziury w całym. Nie ma.",
      "Wzorowa noc. Wpiszę ją sobie do CV.", "Wstałem pierwszy. Jak zawsze.", "Noc za nami. Wskazówki w komplecie."}},
    // S2b (zwykła seria, od SERIA_MIN nocy): spokojny ton, bez medali i świeczek
    {"seria",
     {"Licznik się kręci. Ja też.", "Tykam z dumą."},
     {"Seria dłuższa niż moja lista spóźnień.", "Jestem nakręcony tą serią.", "Tyle nocy, że muszę dokręcić sprężynę.",
      "Noc za nocą. Nudno i dobrze."}},
    // kamień milowy (S-K-28): tu zostają medal i świeczki
    {"kamien",
     {"Medal mi się należy. Tobie też.", "Tykam z dumą.", "Licznik się kręci. Ja też.", "Świeczki zdmuchnąłem. Maski nie."},
     {"Seria dłuższa niż moja lista spóźnień.", "Jestem nakręcony tą serią.", "Tyle nocy, że muszę dokręcić sprężynę.",
      "Kolejna noc. Mój mechanizm się wzruszył."}},
    {"brak_danych",
     {"Zaspałem? Nie, to nie ja.", "Dane się spóźniają. Ja nigdy.", "Czekam. To umiem najlepiej.", "Halo, AirBridge?"},
     {"Aparat milczy. Wreszcie mam ostatnie słowo.", "Poszedłbym po dane. Mam nawet buty.",
      "Dane się spóźniają. Wstyd mi za kolegów.", "Most milczy. Zadzwoniłbym, tylko do kogo?"}},
    // S10 „Synchronizuję się z aparatem” (R-EKR-57): tylko propozycje, do akceptacji (WY15)
    {"synchronizacja",
     {},
     {"Wciągam noc z aparatu. Nie poganiaj.", "Chwilka. Dane jeszcze w kapciach.",
      "Synchronizuję się. Brzmi mądrze, prawda?", "Mieszam kawę. Dane się ładują.",
      "Aparat oddaje noc. Ja podaję rękę."}},
    // pozostałe pule 5.7.5 dosłownie ze spec (kontrola: tools/test_pule_humoru.py)
    // tydzień (S-K-18)
    {"tydzien",
     {
      "Wszystko odhaczone. Sprawdziłem dwa razy."},
     {
      "Tydzień zamknięty. Ołówek odpoczywa.",
      "Lista gotowa. Ja też, prawie.",
      "Tydzień podsumowany. Ja nakręcony.",
      "Niedziela. Nawet ja zwalniam tempo.",
      "Zrobiłem notatki. Na marginesie.",
      "Przeglądam tydzień jak album.",
      "Kolejny tydzień w kalendarzu. Ładny."}},
    // powrót do rytmu (S-K-29)
    {"powrot",
     {
      "Rozgrzewka zaliczona."},
     {
      "Wróciliśmy. Ja nigdzie nie szedłem.",
      "Znów w rytmie. Tykam do taktu.",
      "Rozgrzałem wskazówki na powitanie.",
      "Dobrze, że jesteś. Ja tu stałem.",
      "Trucht zaliczony, teraz marsz.",
      "Powrót jak w filmie. Bez napisów.",
      "Stęskniłem się. Tylko trochę."}},
    // wyciek (S-K-10)
    {"wyciek",
     {
      "Syczy. To nie ja, sprawdzałem.",
      "Coś gwiżdże, a ja nie umiem.",
      "Taśma tu nie pomoże.",
      "Szukam dziury w całym. Dosłownie."},
     {
      "Maska puściła parę. Ja nikomu nie powiem.",
      "Nawet ja raz zadzwoniłem w niedzielę.",
      "Coś uciekało w nocy. Na pewno nie czas.",
      "Syczało? Ja tylko tykam, przysięgam."}},
    // przerwy w nocy (S-K-16)
    {"przerwy",
     {},
     {
      "Rura mnie owinęła. Walczę dalej.",
      "Rozplątuję węzły. Idzie mi średnio.",
      "Wąż ogrodowy to przy tym pestka.",
      "Kabel, rura i ja. Trudny trójkąt.",
      "Wyplątałem się. Prawie elegancko.",
      "Moje ręce do węzłów nie pasują.",
      "Zrobiłem supeł. Dumny nie jestem."}},
    // sprzęt: zużycie (S-K-31)
    {"zuzycie",
     {
      "Ta poduszka widziała już wszystko."},
     {
      "Mam klucz. Nie wiem jeszcze, do czego.",
      "Poduszka na emeryturę. Ja jeszcze nie.",
      "Stare dobre czasy. Za stare.",
      "Majsterkuję. Proszę nie patrzeć.",
      "Wymiana to nie zdrada. Sprawdzałem.",
      "Mnie nikt nie wymienia. Na szczęście.",
      "Lupa mówi jedno: czas na nową."}},
    // sprzęt: po wymianie (S-K-32)
    {"po_wymianie",
     {
      "Miłość od pierwszego uszczelnienia."},
     {
      "Nowa poduszka. Trochę zazdroszczę.",
      "Pasuje jak ulał. Mnie nic nie pasuje.",
      "Cisza w masce. Słyszę tylko siebie.",
      "Szczelnie i spokojnie. Lubię to.",
      "Wymiana się udała. Biorę zasługi.",
      "Ani syku. Mogę wreszcie tykać.",
      "Przytulam maskę. Nie mów nikomu."}},
    // wieczór: wietrzenie (S-K-01)
    {"wietrzenie",
     {
      "Ja otworzę. Ty dopilnuj."},
     {
      "Powietrze stoi. Nie znoszę, jak coś stoi.",
      "Do klamki nie sięgam. Ale kibicuję.",
      "Duszno jak w szufladzie zegarmistrza.",
      "Przeciąg mnie nie ruszy. Mam ciężkie buty.",
      "Otworzyłbym okno, ale mam tylko wskazówki.",
      "Powietrze stoi. Ja przynajmniej chodzę.",
      "Okno otwarte, ja na straży."}},
    // wieczór: smog (S-K-02)
    {"smog",
     {},
     {
      "Zamykam okno. Smog zostaje za nim.",
      "Trzymam się za nos. Profilaktycznie.",
      "Na dworze szaro. W środku ja.",
      "Krótko wietrzę, szybko zamykam.",
      "Smog za szybą. Ja pilnuję szyby.",
      "Wietrzę jak ninja: szybko i cicho.",
      "Za oknem mgła. Nie ta romantyczna.",
      "Szyba to dziś mój najlepszy kolega."}},
    // wieczór: chłód (S-K-03)
    {"chlod",
     {},
     {
      "Zimno mi w dzwonki.",
      "Czapka na głowę, otulina na rurę.",
      "Szczękam wskazówkami. Z zimna.",
      "Rura marznie pierwsza. Potem ja.",
      "Robię na drutach. Dla rury.",
      "Tryb zimowy: szalik i cierpliwość.",
      "Chucham na dzwonki. Pomaga średnio."}},
    // wieczór: suche powietrze (S-K-04)
    {"suche_powietrze",
     {},
     {
      "Szukam kropli. Znalazłem kurz.",
      "Sucho jak na pustyni. Mam konewkę.",
      "Kaktus by się tu odnalazł. Ja nie.",
      "Nawilżam, co mogę. Głównie siebie.",
      "Powietrze szeleści. Nie lubię tego.",
      "Kropla tu, kropla tam. Pomagam.",
      "Wysycham z tęsknoty za wilgocią.",
      "Konewka w dłoń. Ogrodnik ze mnie."}},
    // wieczór: upał (S-K-05)
    {"upal",
     {
      "Wachluję się za nas dwóch."},
     {
      "Topię się. Wskazówki mi miękną.",
      "Gorąco. Tykam w zwolnionym tempie.",
      "Szukam cienia. Jestem przy oknie.",
      "Wentylator to mój nowy przyjaciel.",
      "Pocę się olejem. Zegarmistrz wie.",
      "Za ciepło na pośpiech. Tykam leniwie.",
      "Lato w sypialni. Ja wolę wiosnę."}},
    // wieczór: pyłek (S-K-06)
    {"pylek",
     {},
     {
      "Kwiatki ładne. Z daleka.",
      "Pyłek fruwa, ja się chowam.",
      "Brzoza kwitnie. Trzymam dystans.",
      "Wietrzę na chwilkę. Pyłek czuwa.",
      "Wiosna przyszła. Z pyłkiem w gratisie.",
      "Sprzątam pyłek z dzwonków.",
      "Natura szaleje. Ja tylko tykam.",
      "Pyłek na wskazówkach. Ciężko się kręci."}},
    // wieczór: stała pora (S-K-07)
    {"stala_pora",
     {},
     {
      "Lubię rutynę. To moja praca.",
      "Ta sama pora, ten sam ja.",
      "Punktualność to moje drugie imię.",
      "Moje wskazówki znają tę godzinę.",
      "Godzina jak w zegarku. Dosłownie.",
      "Wieczór o czasie. Jestem wzorem.",
      "Ja zawsze o czasie. Zawodowo.",
      "Zawsze o tej samej. Sprawdzone."}},
    // wieczór: dziś znowu z maską (S-K-08)
    {"z_maska",
     {},
     {
      "Maska podana. Do usług.",
      "Podaję maskę na srebrnej tacy.",
      "Wczoraj przerwa, dziś premiera.",
      "Kelner gotowy. Maska też.",
      "Nowy wieczór, czysta karta.",
      "Maska czeka. Ja też umiem czekać.",
      "Polerowałem tacę cały dzień.",
      "Stolik dla dwojga: ty i maska."}},
    // wieczór: weekend (S-K-09)
    {"weekend",
     {
      "Urlop od budzenia? Nie dla mnie."},
     {
      "Pakuję walizkę. Maska jedzie pierwsza.",
      "W sobotę też tykam. Taki los.",
      "Hamak czeka. Ja w gotowości.",
      "Weekend! A ja nadal w pracy.",
      "Leniuchuję dopiero od niedzieli.",
      "Piątek. Wskazówki już tańczą.",
      "Tryb weekendowy: tykam w kapciach."}},
    // odkrycie (S1 (O-03…O-12), S-K-21)
    {"odkrycie",
     {
      "Mam trop. Prowadzi do okna.",
      "Śledztwo zamknięte.",
      "Liczyłem. Dwa razy.",
      "Zapisz to sobie. Ja już zapisałem."},
     {
      "Wiedziałem, że coś tyka w tych danych.",
      "Panel liczył, ja udawałem, że też liczę.",
      "Zapisałem odkrycie. Nazwę je swoim imieniem.",
      "Zaskoczyło mi w trybikach. Mamy trop."}},
    // przerywnik (S2d (sceny bez własnego zdania))
    {"przerywnik",
     {
      "Kręcenie własnymi wskazówkami boli.",
      "Wstaję, bo muszę. Ty też.",
      "Maska trzyma. Płetwy zbędne."},
     {
      "Dziś bez wiadomości. Sam jestem wiadomością.",
      "Wolne od wykresów. Korzystam.",
      "Nic się nie dzieje. Celebruję to.",
      "Mam wolne. Nadal tykam.",
      "Dzień bez misji. Mam hobby.",
      "Spokój. Ćwiczę bycie ozdobą.",
      "Dziś tylko ja i moje dzwonki.",
      "Odpoczywam aktywnie. Tykam."}},
    // „Zdania scen przerywników” (S2d, propozycja v2): zdanie numer i przypięte do sceny G(i+1), kolejno G01…G18
    {"przerywnik_sceny",
     {},
     {
      "Ktoś przesuwa czas, a pocę się ja.",
      "Punktualność mam w umowie. Entuzjazmu nie.",
      "Odpoczywam od punktualności. Punktualnie.",
      "Postanowienie: więcej dzwonić. Przepraszam.",
      "Kupiłem aparatowi prezent. Z bólem sprężyny.",
      "Jajko czy budzik? Ja zdążyłem przed kurą.",
      "Zakwitnie, kiedy zadzwonię. Taki mamy układ.",
      "Czas leci. Ja prawie też.",
      "Ulepiłem siebie. Wyszedłem na bałwana.",
      "Ja liczę owce, owce liczą na mnie.",
      "Drzemka? Po moim mechanizmie.",
      "Przebrałem się za konkurencję. Wyszło lepiej.",
      "Księżyc ma przewagę. Gra nocą u siebie.",
      "Puchate kapcie. Wizerunek twardziela padł.",
      "Moja orkiestra nie fałszuje. Nie gra.",
      "Hantle? Ja codziennie podnoszę ludzi.",
      "Wodoodporny? Zaraz się okaże.",
      "Kucharz ze mnie żaden. Minutnik świetny."}},
};

static const char* const USPOKAJAJACE[] = {
    "Nic nie dzwoniło. Poza mną.", "Przespane. Ja tylko tykałem.", "Aparat pracował, a medal biorę ja.",
    "Tak cicho, że słyszałem własne tykanie.", "Szukałem dziury w całym. Nie ma.", "Wzorowa noc. Wpiszę ją sobie do CV.",
    "Wszystko odhaczone. Sprawdziłem dwa razy."};

const int KAMIENIE[5] = {7, 30, 100, 180, 365};
const int SERIA_MIN = 7;  // [PRÓG] obserwacja własna, 1 osoba: seria 2 nocy to nie wydarzenie

static std::vector<std::string> dziel(const std::string& l, int n) {
  std::vector<std::string> p;
  size_t start = 0;
  for (size_t i = 0; i <= l.size(); i++)
    if (i == l.size() || l[i] == ';') { p.push_back(l.substr(start, i - start)); start = i + 1; }
  if ((int)p.size() != n) p.clear();
  return p;
}

bool parsujHumor(const std::string& l0, WpisHumoru& w) {
  std::string l = l0;
  if (!l.empty() && l.back() == '\r') l.pop_back();
  auto p = dziel(l, 6);
  if (p.empty() || !dataOk(p[0]) || p[1].empty() || p[3].size() != 1 || (p[3][0] != 'K' && p[3][0] != 'D')) return false;
  w = {p[0], p[1], p[2], p[3][0], p[4], p[5]};
  return true;
}

std::string zapiszHumor(const WpisHumoru& w) {
  return w.data + ";" + w.typ + ";" + w.klucz + ";" + w.tryb + ";" + w.scena + ";" + w.zdanie;
}

const WpisHumoru* znajdzWpis(const std::vector<WpisHumoru>& log, const std::string& data, const std::string& typ,
                             const std::string& klucz) {
  for (auto it = log.rbegin(); it != log.rend(); ++it)
    if (it->data == data && it->typ == typ && it->klucz == klucz) return &*it;
  return nullptr;
}

const Pula* pula(const std::string& id) {
  for (auto& p : PULE)
    if (id == p.id) return &p;
  return nullptr;
}

bool uspokajajace(const std::string& z) {
  for (auto u : USPOKAJAJACE)
    if (z == u) return true;
  return false;
}

std::vector<std::string> zdaniaDoLosowania(const Pula& p, bool zPropozycjami, bool uspokojenie) {
  std::vector<std::string> w;
  int i = 0;
  auto dodaj = [&](const char* z) {
    if (uspokojenie || !uspokajajace(z)) w.push_back(std::string(p.id) + ":" + std::to_string(i));
    i++;
  };
  for (auto z : p.kanwa) dodaj(z);
  if (zPropozycjami)
    for (auto z : p.propozycje) dodaj(z);
  else
    i += (int)p.propozycje.size();
  return w;
}

std::string tekstZdania(const std::string& id) {
  size_t k = id.find(':');
  if (k == std::string::npos) return "";
  const Pula* p = pula(id.substr(0, k));
  if (!p) return "";
  // numer zdania pochodzi z `/humor.csv` (plik na flash): kontrola zakresu przed indeksem
  int i = 0;
  if (!indeksZTekstu(id.c_str() + k + 1, (int)(p->kanwa.size() + p->propozycje.size()), i)) return "";
  if (i < (int)p->kanwa.size()) return p->kanwa[(size_t)i];
  return p->propozycje[(size_t)i - p->kanwa.size()];
}

// Tabela 5.7.3 (kolumny „Tryb”, „Oszczędny”, „Sceny”) i pule 5.7.5 dla każdego typu ekranu z kartą.
static const std::vector<const char*> SCENY_D = {"D01", "D02", "D03", "D04", "D05", "D06", "D07", "D08"};  // K22
static const std::vector<const char*> SCENY_G = {"G01", "G02", "G03", "G04", "G05", "G06", "G07", "G08", "G09",
                                                 "G10", "G11", "G12", "G13", "G14", "G15", "G16", "G17", "G18"};
const std::vector<TypKarty>& typyKart() {
  using R = RegulaKarty;
  static const std::vector<TypKarty> t = {
      {"S2A", R::CO_TRZECIE, false, false, "dobra_noc", SCENY_D},
      {"S2C", R::CO_TRZECIE, false, false, "dobra_noc", SCENY_D},
      {"S2B", R::ZAWSZE, false, false, "seria", {"D02", "D05", "D07", "D08"}},  // K01: bez medalu
      {"S2D", R::PRZERYWNIK, false, false, "przerywnik", SCENY_G},                  // scenę wybiera wybierzPrzerywnik()
      {"KM", R::ZAWSZE, true, false, "kamien", {}},                             // S-K-28: scena z liczby nocy
      {"SK27", R::ZAWSZE, true, false, "kamien", {"M02"}},
      {"SK29", R::ZAWSZE, true, false, "powrot", {"M07"}},                      // K04
      {"SK18", R::ZAWSZE, false, false, "tydzien", {"M06", "D03", "D06"}},      // K03
      {"S1", R::ZAWSZE, true, false, "odkrycie", {}},                           // K13: scena z ID odkrycia (6.5)
      {"SK21", R::ZAWSZE, true, false, "odkrycie", {"O04"}},                    // K13 poza wariantem „pyłek”
      {"SK32", R::ZAWSZE, true, false, "po_wymianie", {"S02"}},                 // K14
      {"SK01", R::JEDEN_NA_DWA, false, false, "wietrzenie", {"W01"}},           // K05
      {"SK02", R::JEDEN_NA_DWA, false, false, "smog", {"W02"}},
      {"SK03", R::JEDEN_NA_DWA, false, false, "chlod", {"W03"}},                // K06
      {"SK04", R::JEDEN_NA_DWA, false, false, "suche_powietrze", {"W05"}},      // K08
      {"SK05", R::JEDEN_NA_DWA, false, false, "upal", {"W04"}},                 // K07
      {"SK06", R::JEDEN_NA_DWA, false, false, "pylek", {"W07"}},
      {"SK07", R::JEDEN_NA_DWA, false, false, "stala_pora", {"W08"}},
      {"SK08", R::JEDEN_NA_DWA, false, false, "z_maska", {"W10", "P06"}},       // K12
      {"SK09", R::JEDEN_NA_DWA, false, false, "weekend", {"W09"}},              // K15
      {"SK10", R::JEDEN_NA_DWA_PROBLEM, false, true, "wyciek", {"P01", "P03"}},  // K09
      {"SK16", R::JEDEN_NA_DWA_PROBLEM, false, true, "przerwy", {"P04"}},        // K10
      {"SK31", R::JEDEN_NA_DWA_PROBLEM, false, true, "zuzycie", {"S01", "P05"}}, // K11
      {"S5", R::PIERWSZY_BRAK, false, false, "brak_danych", {"B04"}},           // K16: lupa, budzik szuka zgubionej nocy
      {"S10", R::ZAWSZE, true, false, "synchronizacja", {"B05"}},               // K23: noc w drodze (R-EKR-57)
  };
  return t;
}

const TypKarty* typKarty(const std::string& typ) {
  for (auto& t : typyKart())
    if (typ == t.typ) return &t;
  return nullptr;
}

// Odkrycie S1 (klucz `O-xx` albo `O-12/{kod czynnika}`): scena z przydziału 6.5 i czy jest kartą (5.7.3, R-HUM-25).
static std::string idOdkrycia(const std::string& klucz) { return klucz.substr(0, klucz.find('/')); }
static bool odkrycieKarta(const std::string& klucz) {
  std::string id = idOdkrycia(klucz);
  for (auto o : {"O-03", "O-04", "O-06", "O-07", "O-08", "O-09"})
    if (id == o) return klucz == id;
  // O-12 kartą tylko przy czynnikach „stres” (S), „późna, ciężka kolacja” (J), „spanie poza domem” (P)
  return id == "O-12" && (klucz == "O-12/S" || klucz == "O-12/J" || klucz == "O-12/P");
}

std::vector<std::string> scenyTypu(const std::string& typ, const std::string& klucz) {
  if (typ == "KM") {
    int n = 0;
    if (!liczbaKlucza(klucz, n)) return scenyTypu("S2A", "");  // uszkodzony klucz: jak trescDnia(), zwykły ekran dnia
    if (n <= 30) return {"M01"};
    if (n == 100) return {"M03"};
    if (n == 180) return {"M04"};
    return {"M05"};  // 365 nocy i rocznica
  }
  if (typ == "S1") {
    std::string id = idOdkrycia(klucz);
    if (id == "O-04") return {"O02"};
    if (id == "O-08" || id == "O-09") return {"O03"};
    if (id == "O-06" || id == "O-07") return {"O04"};
    if (id == "O-03") return {"M08"};  // najlepszy tydzień
    if (id == "O-12") return {"O01"};
    return {};
  }
  const TypKarty* t = typKarty(typ);
  if (!t) return {};
  return std::vector<std::string>(t->sceny.begin(), t->sceny.end());
}

const int KLUCZ_MAX = 1000000;

bool liczbaKlucza(const std::string& klucz, int& n) { return indeksZTekstu(klucz.c_str(), KLUCZ_MAX + 1, n); }

const char* pulaTypu(const std::string& typ) {
  const TypKarty* t = typKarty(typ);
  return t ? t->pula : "";
}

bool krokDoLekarza(const std::string& krok) {
  for (auto w : {"lekarz", "Lekarz", "kontroli", "Kontroli", "pomoc", "Pomoc"})
    if (krok.find(w) != std::string::npos) return true;
  return false;
}

static bool spokoj(const std::string& t) { return t == "S2A" || t == "S2B" || t == "S2C"; }

EkranDnia wybierzEkranDnia(const StanDnia& s, const std::vector<WpisHumoru>& log) {
  EkranDnia e;
  if (s.brakDanych) {  // R1
    e.typ = "S5";
    e.klucz = s.kluczBraku;
    // R-HUM-08: karta tylko przy pierwszym wyświetleniu tego braku danych i tylko, gdy AirBridge nie odpowiada;
    // pula „brak danych” mówi o spóźnionych danych z AirBridge, więc przy braku Wi-Fi i złym raporcie byłaby nieprawdziwa
    e.kartaZTabeli = s.kartaBraku;
    for (auto& w : log)
      if (w.typ == "S5" && w.klucz == s.kluczBraku) e.kartaZTabeli = false;
    return e;
  }
  if (s.synchronizacja) {  // R-EKR-57: noc w drodze; ekran przejściowy, karta zawsze, bez wpisu w dzienniku
    e.typ = "S10";
    e.kartaZTabeli = e.kartaOszczedny = true;
    return e;
  }
  if (s.nocDzis && s.nocDzisPonad4h) {  // R7: kamień milowy, pokazywany raz
    for (int k : KAMIENIE)
      if (s.nocyPonad4h == k) {
        bool byl = false;
        for (auto& w : log)
          if (w.typ == "KM" && w.klucz == std::to_string(k) && w.data != s.dzis) byl = true;
        if (!byl) {
          e.typ = "KM";
          e.klucz = std::to_string(k);
          e.kartaZTabeli = e.kartaOszczedny = true;
          return e;
        }
      }
  }
  if (s.nocyWaznych < 14 || !s.maNorme) {  // R9: rozruch globalny
    e.typ = "S6";
    return e;
  }
  // R9: dowód spokoju; dzień z wpisem zostaje przy swoim wariancie
  std::string dzisTyp, ostatniTyp;
  int licznikAC = 0;
  for (auto& w : log) {
    if (!spokoj(w.typ)) continue;
    if (w.data == s.dzis) dzisTyp = w.typ;
    else if (w.data < s.dzis) ostatniTyp = w.typ;
    int n = 0;
    if (w.typ != "S2B" && liczbaKlucza(w.klucz, n)) licznikAC = std::max(licznikAC, n);  // zły klucz pominięty
  }
  if (!dzisTyp.empty()) {
    e.typ = dzisTyp;
  } else {
    static const char* const KOL[3] = {"S2A", "S2B", "S2C"};
    int start = 0;
    for (int i = 0; i < 3; i++)
      if (ostatniTyp == KOL[i]) start = (i + 1) % 3;
    e.typ = "S2A";
    for (int k = 0; k < 3; k++) {
      std::string t = KOL[(start + k) % 3];
      if (t == "S2A" && dzienTygodnia(s.dzis) == 0) continue;  // R-EKR-16: niedziela bez S2a
      if (t == "S2B" && s.seria < SERIA_MIN) continue;            // S2b tylko przy M-20 ≥ 7 (R-EKR-16)
      if (t == "S2C" && !s.sypialnia) continue;                  // S2c: norma sypialni i odczyt nocy (5.4.2)
      e.typ = t;
      break;
    }
  }
  if (e.typ == "S2B") {
    e.kartaZTabeli = true;  // R-HUM-19
  } else {
    const WpisHumoru* juz = nullptr;
    for (auto& w : log)
      if (w.data == s.dzis && w.typ == e.typ) juz = &w;
    int nr = licznikAC + 1;
    if (juz && !liczbaKlucza(juz->klucz, nr)) nr = licznikAC + 1;  // wpis z dziś z uszkodzonym kluczem: nowy numer
    e.klucz = std::to_string(nr);
    e.kartaZTabeli = nr % 3 == 0;  // R-HUM-09: co trzecie wyświetlenie S2a albo S2c
  }
  return e;
}

// R-HUM-02 i R-HUM-23 dla wyświetlenia w dniu `dzis`: "" = karta dozwolona, inaczej powód ekranu danych.
static const char* limitKart(const std::string& dzis, const std::vector<WpisHumoru>& log) {
  int kartDzis = 0, kartOkno = 1, wyswOkno = 1;  // +1: to wyświetlenie
  std::string od = dodajDni(dzis, -6);
  for (auto& x : log) {
    if (x.data == dzis && x.tryb == 'K') kartDzis++;
    if (x.data >= od && x.data <= dzis) {
      wyswOkno++;
      if (x.tryb == 'K') kartOkno++;
    }
  }
  if (kartDzis >= 2) return "2 karty na dobę (R-HUM-02)";
  if (kartOkno > 0.5 * std::max(wyswOkno, 7)) return "połowa wyświetleń w 7 dniach (R-HUM-23)";
  return "";
}

// Ostatni dzień, w którym scena `s` była na karcie (dowolny typ ekranu); "" = nigdy.
static std::string ostatniaKarta(const std::string& s, const std::vector<WpisHumoru>& log) {
  std::string ost;
  for (auto& x : log)
    if (x.scena == s && x.tryb == 'K' && x.data > ost) ost = x.data;
  return ost;
}

// Scena z listy `kandydaci` dostępna w pamięci (R-HUM-11) i, przy `limit`, spoza 6 poprzednich dni (R-HUM-10);
// z dopuszczalnych najdawniej pokazana, przy remisie pierwsza na liście. "" = brak.
static std::string najdawniejszaScena(const std::vector<std::string>& kandydaci, const std::string& dzis,
                                      const std::vector<WpisHumoru>& log, const std::vector<std::string>& wPamieci,
                                      bool limit) {
  std::string poprzednie = dodajDni(dzis, -6);
  std::string najScena, najData = "~";
  for (auto& s : kandydaci) {
    if (std::find(wPamieci.begin(), wPamieci.end(), s) == wPamieci.end()) continue;
    std::string ost = ostatniaKarta(s, log);
    if (limit && !ost.empty() && ost >= poprzednie && ost < dzis) continue;  // R-HUM-10
    if (ost < najData) { najData = ost; najScena = s; }
  }
  return najScena;
}

Wybor wybierzTryb(const EkranDnia& e, bool tekstMiesci, const std::vector<WpisHumoru>& logPelny,
                  const KontekstHumoru& k) {
  Wybor w;
  if (e.typ == "S2D") { w.dlaczego = "przerywnik tylko przez wybierzPrzerywnik()"; return w; }
  // to samo wyświetlenie oceniane ponownie (odświeżenie): własny wpis z dziś nie liczy się do limitów ani rotacji
  std::vector<WpisHumoru> log;
  log.reserve(logPelny.size());
  for (auto& x : logPelny)
    if (!(x.data == k.dzis && x.typ == e.typ && x.klucz == e.klucz)) log.push_back(x);
  if (k.humor == Humor::WYLACZONY) { w.dlaczego = "humor wyłączony (R-HUM-03)"; return w; }
  if (k.humor == Humor::OSZCZEDNY && !e.kartaOszczedny) { w.dlaczego = "humor oszczędny (R-HUM-06)"; return w; }
  if (!e.kartaZTabeli) { w.dlaczego = "tabela 5.7.3"; return w; }
  if (k.wzorzecB && (e.typ == "S2A" || e.typ == "S2C")) { w.dlaczego = "wzorzec B (R-HUM-26)"; return w; }
  if (k.liniaUwagi) { w.dlaczego = "linia uwagi (R-HUM-17)"; return w; }
  const Pula* p = pula(pulaTypu(e.typ));
  if (!p) { w.dlaczego = "brak puli"; return w; }
  std::vector<std::string> zd = zdaniaDoLosowania(*p, k.zPropozycjami, k.uspokojenie);
  if (zd.size() < 3) { w.dlaczego = "pula < 3 zdania (R-HUM-27)"; return w; }
  if (!tekstMiesci) { w.dlaczego = "tekst nie mieści się w karcie (R-HUM-05)"; return w; }
  bool przejsciowy = e.typ == "S10";  // R-EKR-57: poza limitami kart 7.4 i limitem scen R-HUM-10
  if (!przejsciowy) {
    const char* limit = limitKart(k.dzis, log);
    if (*limit) { w.dlaczego = limit; return w; }
  }
  // scena: przypisana do ekranu, w pamięci, nie z 6 poprzednich dni; z dopuszczalnych najdawniej pokazana
  std::string najScena = najdawniejszaScena(scenyTypu(e.typ, e.klucz), k.dzis, log, k.sceny, !przejsciowy);
  if (najScena.empty()) { w.dlaczego = "brak sceny (R-HUM-10, R-HUM-11)"; return w; }
  // zdanie najdawniej użyte; nieużyte przed użytymi, remis według kolejności puli (R-HUM-20)
  std::string najZd, najData = "~";
  for (auto& z : zd) {
    std::string ost;
    for (auto& x : log)
      if (x.zdanie == z && x.data > ost) ost = x.data;
    if (ost < najData) { najData = ost; najZd = z; }
  }
  w.tryb = 'K';
  w.scena = najScena;
  w.zdanie = najZd;
  return w;
}

std::string zdanieSynchronizacji(const std::string& dzis, bool zPropozycjami) {
  const Pula* p = pula("synchronizacja");
  if (!p || !dataOk(dzis)) return "";
  std::vector<std::string> zd = zdaniaDoLosowania(*p, zPropozycjami, true);
  if (zd.empty()) return "";
  return tekstZdania(zd[(size_t)nrDnia(dzis) % zd.size()]);
}

// ---------- ekrany z kartą spoza rotacji dnia ----------

EkranDnia ekranKarty(const std::string& typ, const std::string& sprawa, const std::string& dzis,
                     const std::vector<WpisHumoru>& log, const std::string& krok) {
  EkranDnia e;
  e.typ = typ;
  // sprawa trafia do pola `klucz` w `/humor.csv`: bez separatora pól i końca wiersza, inaczej ekran danych bez klucza
  if (sprawa.find_first_of(";\r\n") != std::string::npos) return e;
  e.klucz = sprawa;
  const TypKarty* t = typKarty(typ);
  if (!t || krokDoLekarza(krok)) return e;                      // R-HUM-25
  if (typ == "S1" && !odkrycieKarta(sprawa)) return e;          // R-HUM-25: O-05, O-10, O-11, O-12 wrażliwe
  if (typ == "SK21" && sprawa == "pylek") return e;             // R-HUM-25: krok `→ Zatkany nos? Powiedz lekarzowi.`
  e.kartaOszczedny = t->oszczedny;
  e.tonProblem = t->problem;
  switch (t->regula) {
    case RegulaKarty::ZAWSZE:
      e.kartaZTabeli = true;
      break;
    case RegulaKarty::JEDEN_NA_DWA:
    case RegulaKarty::JEDEN_NA_DWA_PROBLEM: {
      // R-HUM-07: licznik wyświetleń sprawy bez okna czasu; dzień z wpisem zostaje przy swoim numerze
      std::string przed = sprawa + "#";
      int poprzednie = 0, dzisNr = -1;
      char trybPoprzedniego = 0;  // tryb wyświetlenia o najwyższym numerze przed dziś (0 = pierwsze wyświetlenie)
      for (auto& x : log) {
        if (x.typ != typ || x.klucz.compare(0, przed.size(), przed) != 0) continue;
        int n = 0;
        if (!liczbaKlucza(x.klucz.substr(przed.size()), n)) continue;  // uszkodzony numer pominięty
        if (x.data == dzis) dzisNr = n;
        else if (x.data < dzis && n >= poprzednie) { poprzednie = n; trybPoprzedniego = x.tryb; }
      }
      int nr = dzisNr > 0 ? dzisNr : poprzednie + 1;
      e.klucz = przed + std::to_string(nr);
      if (trybPoprzedniego == 0)  // pierwsze: ton „problem” ekranem danych z wykresem, pozostałe kartą
        e.kartaZTabeli = t->regula == RegulaKarty::JEDEN_NA_DWA;
      else  // co drugie: po ekranie danych (także takim, który ustąpił linii uwagi albo limitowi) karta
        e.kartaZTabeli = trybPoprzedniego == 'D';
      break;
    }
    case RegulaKarty::PIERWSZY_BRAK:  // S5: wybierzEkranDnia() zna kartaBraku (E-01 albo E-03, E-05), tu nie
    case RegulaKarty::PRZERYWNIK:     // S2D: tylko wybierzPrzerywnik (bramka, R-HUM-26)
    case RegulaKarty::CO_TRZECIE:     // S2a, S2c: licznik rotacji w wybierzEkranDnia()
      break;
  }
  return e;
}

// ---------- przerywnik S2d ----------

std::string wielkanoc(int rok) {
  // algorytm Meeusa/Jonesa/Butchera (kalendarz gregoriański)
  int a = rok % 19, b = rok / 100, c = rok % 100, d = b / 4, e = b % 4, f = (b + 8) / 25, g = (b - f + 1) / 3;
  int h = (19 * a + b - d - g + 15) % 30, i = c / 4, k = c % 4, l = (32 + 2 * e + 2 * i - h - k) % 7;
  int m = (a + 11 * h + 22 * l) / 451, mies = (h + l - 7 * m + 114) / 31, dzien = (h + l - 7 * m + 114) % 31 + 1;
  char t[16];
  snprintf(t, sizeof t, "%04d-%02d-%02d", rok % 10000, mies, dzien);
  return t;
}

std::string okazjaDnia(const std::string& dzis) {
  if (!dataOk(dzis)) return "";
  int rok = liczbaCalk(dzis.substr(0, 4).c_str()), mies = liczbaCalk(dzis.substr(5, 2).c_str()),
      dzien = liczbaCalk(dzis.substr(8, 2).c_str());
  // kolejność według ID sceny: przy dwóch okazjach tego samego dnia wygrywa niższe ID (5.7.6)
  // G01 na obie zmiany czasu (ostatnia niedziela marca i października);
  // w marcu neutralna G01 ma osobny tekst (`Dzisiaj śpisz godzinę krócej.`), bo październikowy mówi „zyskaliśmy godzinę”.
  // 28.03.2027 to też Wielkanoc (G06): wygrywa niższe ID, czyli G01
  if ((mies == 10 || mies == 3) && dzien >= 25 && dzienTygodnia(dzis) == 0) return "G01";
  if (mies == 1 && dzien == 1) return "G04";
  if (mies == 12 && dzien == 25) return "G05";
  if (dzis == wielkanoc(rok)) return "G06";
  if (mies == 3 && dzien == 21) return "G07";
  if (mies == 9 && dzien == 23) return "G08";
  return "";
}

// Nazwa okazji z kolumny „Okazja” tabeli 5.7.6 (wielka litera na początku; etykieta ekranu przechodzi rundę).
static const char* nazwaOkazji(const std::string& scena) {
  static const char* const N[][2] = {{"G01", "Zmiana czasu"}, {"G02", "Poniedziałek"}, {"G03", "Piątek"},
                                     {"G04", "Nowy Rok"},     {"G05", "Boże Narodzenie"}, {"G06", "Wielkanoc"},
                                     {"G07", "Pierwszy dzień wiosny"}, {"G08", "Jesień"}};
  for (auto& n : N)
    if (scena == n[0]) return n[1];
  return "";
}

// Zdanie przypięte do sceny przerywnika (5.7.5): z propozycjami zdanie z tabeli „Zdania scen przerywników”
// (`przerywnik_sceny:{i}`), bez nich tylko zdania z kanwy K17 (G01), K18 (G02), K20 (G12). "" = scena bez zdania,
// nie jest losowana (R-HUM-21 fail-path, R-HUM-27).
static std::string zdanieSceny(const std::string& scena, bool zPropozycjami) {
  int i = -1;
  for (size_t j = 0; j < SCENY_G.size(); j++)
    if (scena == SCENY_G[j]) i = (int)j;
  if (i < 0) return "";
  if (zPropozycjami) return "przerywnik_sceny:" + std::to_string(i);
  if (scena == "G01") return "przerywnik:0";
  if (scena == "G02") return "przerywnik:1";
  if (scena == "G12") return "przerywnik:2";
  return "";
}

Przerywnik wybierzPrzerywnik(const EkranDnia& dowod, const std::vector<WpisHumoru>& log, const KontekstHumoru& k,
                             const UstawieniaPrzerywnikow& u) {
  Przerywnik r;
  auto bez = [&](const char* powod) {
    r = Przerywnik();
    r.dlaczego = powod;
    return r;
  };
  if (!u.tekstyPrzyjete) return bez("teksty przerywników nieprzyjęte");
  if (k.humor != Humor::NORMALNY) return bez("humor (R-HUM-16)");
  if (!spokoj(dowod.typ)) return bez("nie dowód spokoju (5.7.6)");
  if (k.wzorzecB) return bez("wzorzec B (R-HUM-26)");
  if (k.liniaUwagi) return bez("linia uwagi (R-HUM-22)");
  for (auto it = log.rbegin(); it != log.rend(); ++it) {
    if (it->data != k.dzis) continue;
    if (it->typ == "S2D" && it->tryb == 'K') {  // ten sam dzień: ten sam przerywnik
      r.jest = true;
      r.scena = it->scena;
      r.zdanie = it->zdanie;
      r.klucz = it->klucz;
      r.okazja = nazwaOkazji(it->scena);
      r.warunkowa = warunekPrzerywnika(it->scena, k.noc);  // R-HUM-28: wariant z faktów nocy, nie z dziennika
      return r;
    }
    if (spokoj(it->typ)) return bez("dzień z dowodem spokoju");
  }
  std::string scena;
  std::string okazja = u.zDatami ? okazjaDnia(k.dzis) : "";
  if (!okazja.empty()) {  // okazja ze stałą datą: poza limitem 1 na 7 dni, w limitach kart (R-HUM-15)
    scena = okazja;
    r.klucz = "O";
  } else {
    std::string ost;
    for (auto& x : log)
      if (x.typ == "S2D" && x.tryb == 'K' && x.data < k.dzis && x.data > ost) ost = x.data;
    if (!ost.empty() && nrDnia(k.dzis) - nrDnia(ost) < 7) return bez("1 na 7 dni (R-HUM-14)");
    r.klucz = "L";
    int dt = dzienTygodnia(k.dzis);
    {
      std::vector<std::string> ogolne = u.ogolne;
      if (ogolne.empty())
        for (size_t j = 8; j < SCENY_G.size(); j++) ogolne.push_back(SCENY_G[j]);  // G09–G18
      // losowanie spośród scen dozwolonych dziś:
      // pula ogólna, G02 tylko w poniedziałek, G03 tylko w piątek; scena ze zdaniem, w pamięci (R-HUM-11), spoza
      // 6 poprzednich dni (R-HUM-10). Ziarno = numer dnia: ten sam wynik na panelu i w symulatorze; własny
      // Fisher–Yates na surowym wyjściu mt19937 (std::shuffle zależy od biblioteki)
      if (u.zDatami && dt == 1) ogolne.push_back("G02");
      if (u.zDatami && dt == 5) ogolne.push_back("G03");
      std::vector<std::string> dozwolone;
      std::string poprzednie = dodajDni(k.dzis, -6);
      for (auto& s : ogolne) {
        if (zdanieSceny(s, k.zPropozycjami).empty()) continue;
        if (std::find(k.sceny.begin(), k.sceny.end(), s) == k.sceny.end()) continue;
        std::string ostS = ostatniaKarta(s, log);
        if (!ostS.empty() && ostS >= poprzednie && ostS < k.dzis) continue;
        dozwolone.push_back(s);
      }
      std::sort(dozwolone.begin(), dozwolone.end());
      std::mt19937 los((uint32_t)nrDnia(k.dzis));
      for (size_t i = dozwolone.size(); i > 1; i--) std::swap(dozwolone[i - 1], dozwolone[(size_t)(los() % (uint32_t)i)]);
      scena = dozwolone.empty() ? std::string() : dozwolone[0];
      if (scena.empty()) return bez("brak sceny ze zdaniem (R-HUM-10, R-HUM-11, R-HUM-27)");
    }
  }
  if (std::find(k.sceny.begin(), k.sceny.end(), scena) == k.sceny.end()) return bez("brak sceny (R-HUM-11)");
  std::string ost = ostatniaKarta(scena, log);
  if (!ost.empty() && ost >= dodajDni(k.dzis, -6) && ost < k.dzis) return bez("scena z 6 poprzednich dni (R-HUM-10)");
  r.zdanie = zdanieSceny(scena, k.zPropozycjami);
  if (r.zdanie.empty()) return bez("scena bez zdania (R-HUM-27)");
  const char* limit = limitKart(k.dzis, log);
  if (*limit) return bez(limit);
  r.jest = true;
  r.scena = scena;
  r.okazja = nazwaOkazji(scena);
  r.warunkowa = warunekPrzerywnika(scena, k.noc);  // po wyborze sceny (R-HUM-29): warunek spełniony → tekst warunkowy
  return r;
}

UstawieniaPrzerywnikow ustawieniaPrzerywnikowBety() {
  UstawieniaPrzerywnikow u;
  u.tekstyPrzyjete = true;  // teksty przyjęte do wersji beta
  u.zDatami = true;         // sceny z datami G01–G08 i poniedziałek/piątek (5.7.6, R-HUM-15)
  return u;
}

TekstPrzerywnika tekstPrzerywnika(const std::string& scena, bool warunkowa, const std::string& dzis) {
  TekstPrzerywnika t;
  if (std::find(SCENY_G.begin(), SCENY_G.end(), scena) == SCENY_G.end()) return t;
  // neutralna G01 w marcu (wiosenna zmiana czasu, godzina znika) ma własny tekst
  bool marzec = scena == "G01" && !warunkowa && dataOk(dzis) && dzis.compare(5, 2, "03") == 0;
  std::string p = "S2D." + scena + (warunkowa ? ".warunek." : marzec ? ".neutralna_marzec." : ".neutralna.");
  t.tytul = sk::tekst(p + "tytul");
  t.powod = sk::tekst(p + "powod");
  return t;
}

// ---------- warunki kart przerywników (R-HUM-28) ----------

NocPrzerywnika nocPrzerywnika(const std::vector<Noc>& hist, const std::string& dzis, bool stanAhi) {
  NocPrzerywnika r;
  r.stanAhi = stanAhi;
  const Noc* n = nocZDnia(hist, dzis);
  if (!n || n->czesciowa || n->m01 <= 0) return r;  // brak nocy dziś, noc częściowa albo bez terapii: brak danych
  r.jest = true;
  r.m01 = n->m01;
  r.wazna = n->wazna;
  r.wyciekP95 = n->wp95;
  r.ahi = n->ahi;
  r.ocenione = n->wazna && !nocNiepewna(*n) && !std::isnan(n->ahi);
  // tydzień: noce ważne, niepewne wyłączone jak z normy AHI (R-NOR-19), z AHI, w 7 dniach do dziś włącznie
  std::vector<double> v;
  std::string od = dodajDni(dzis, -6);
  for (auto& x : hist)
    if (x.data >= od && x.data <= dzis && x.wazna && !x.czesciowa && !nocNiepewna(x) && !std::isnan(x.ahi)) v.push_back(x.ahi);
  r.noce7 = (int)v.size();
  r.medianaAhi7 = mediana(v);
  for (double x : v) r.maxAhi7 = std::isnan(r.maxAhi7) ? x : std::max(r.maxAhi7, x);
  r.m08 = n->m08;
  {  // mediana M-01 z 7 poprzednich nocy ważnych (dni dzis-7 … dzis-1)
    std::vector<double> m;
    std::string odP = dodajDni(dzis, -7);
    for (auto& x : hist)
      if (x.data >= odP && x.data < dzis && x.wazna && !x.czesciowa && x.m01 > 0) m.push_back((double)x.m01);
    r.noceM01Poprz = (int)m.size();
    r.medianaM01Poprz = mediana(m);
  }
  const Noc* pop = nocZDnia(hist, dodajDni(dzis, -1));
  r.poprzedniaWazna = pop && pop->wazna && !pop->czesciowa;
  // E-23 w 7 dniach: wyciek P95 nocy z terapią co najmniej E-23.przeciek_p95; noce niepewne też (to ich powód)
  for (auto& x : hist)
    if (x.data >= od && x.data <= dzis && !x.czesciowa && x.m01 > 0 && !std::isnan(x.wp95) && x.wp95 >= PRZ_WYCIEK_E23) r.problem7 = true;
  return r;
}

bool duzeWycieki(const NocPrzerywnika& n) {
  return n.jest && !std::isnan(n.wyciekP95) && n.wyciekP95 > PROG_WYCIEK && n.wazna;
}
// AHI tak, jak widzi je pacjent na karcie (`przecinek1`, jedno miejsce po przecinku); `5,0` nie stoi pod „w celu”
static double ahiNaKarcie(double ahi) { return std::floor(ahi * 10 + 0.5 + 1e-4) / 10; }  // jak zaokr() w kokpit.cpp
bool podwyzszoneAhi(const NocPrzerywnika& n) { return n.ocenione && n.ahi >= PROG_AHI; }
bool niskieAhi(const NocPrzerywnika& n) { return n.ocenione && ahiNaKarcie(n.ahi) < PROG_AHI; }
bool bardzoNiskieAhi(const NocPrzerywnika& n) { return n.ocenione && n.ahi < PRZ_AHI_BARDZO_NISKIE; }
bool stabilneAhi(const NocPrzerywnika& n) { return niskieAhi(n) && !n.stanAhi; }
bool duzoZdarzen(const NocPrzerywnika& n) { return n.ocenione && n.ahi >= PRZ_AHI_DUZO; }
bool krotkaTerapia(const NocPrzerywnika& n) { return n.jest && n.m01 > 0 && n.m01 < PRZ_M01_KROTKA_MIN; }
bool dlugaTerapia(const NocPrzerywnika& n) {
  // „długi czas” = wyraźna poprawa; sam próg 7 h nie wystarcza komuś z normą ok. 9 h
  return n.jest && n.wazna && n.m01 >= PRZ_M01_DLUGA_MIN && n.noceM01Poprz >= PRZ_M01_POPRAWA_NOCE && !std::isnan(n.medianaM01Poprz) &&
         n.m01 - n.medianaM01Poprz >= PRZ_M01_POPRAWA_MIN - 1e-9;
}
bool dobraTerapia(const NocPrzerywnika& n) {
  return niskieAhi(n) && !n.stanAhi && !std::isnan(n.wyciekP95) && n.wyciekP95 < PROG_WYCIEK && n.m01 >= PRZ_M01_KROTKA_MIN;
}
bool dobryTydzien(const NocPrzerywnika& n) {
  return n.ocenione && niskieAhi(n) && n.noce7 >= PRZ_TYDZIEN_NOCE && !std::isnan(n.medianaAhi7) && n.medianaAhi7 < PROG_AHI &&
         !std::isnan(n.maxAhi7) && n.maxAhi7 < PRZ_AHI_DUZO && !n.problem7;
}
bool bezZarzutu(const NocPrzerywnika& n) { return dobraTerapia(n) && n.m01 >= PRZ_M01_W_CELU_MIN; }
bool wszystkoWCelu(const NocPrzerywnika& n) {
  // M-08 niezmierzone nie blokuje: P95 do 10 L/min i tak wyklucza dłuższy czas ponad 24 L/min
  bool m08Maly = std::isnan(n.m08) || (n.m08 < PRZ_M08_MINUTY_E20 && n.m08 < PRZ_M08_ODSETEK_E20 * n.m01);
  return bezZarzutu(n) && n.wyciekP95 <= PRZ_WYCIEK_NIEWIELKI && m08Maly;
}
bool stabilnyTydzien(const NocPrzerywnika& n) {
  return stabilneAhi(n) && n.noce7 >= PRZ_TYDZIEN_NOCE && !std::isnan(n.maxAhi7) && ahiNaKarcie(n.maxAhi7) < PROG_AHI;
}
bool solidnaNoc(const NocPrzerywnika& n) {
  return dlugaTerapia(n) && niskieAhi(n) && !std::isnan(n.wyciekP95) && n.wyciekP95 < PROG_WYCIEK && n.poprzedniaWazna;
}

bool warunekPrzerywnika(const std::string& scena, const NocPrzerywnika& n) {
  if (!n.jest) return false;
  // karta pochwalna mówi dobrze o terapii, więc ma wariant z warunkiem tylko przy spełnionym uspokojeniu
  // 5.4.13 (R-EKR-32); inaczej karta neutralna tej samej sceny
  static const char* const POCHWALNE[] = {"G02", "G03", "G04", "G05", "G06", "G08", "G13", "G15", "G16", "G18"};
  if (!n.uspokojenie)
    for (const char* p : POCHWALNE)
      if (scena == p) return false;
  if (scena == "G01" || scena == "G07" || scena == "G12" || scena == "G17") return duzeWycieki(n);
  if (scena == "G02" || scena == "G04") return dobraTerapia(n);
  if (scena == "G18") return bezZarzutu(n);
  if (scena == "G03") return dobryTydzien(n);
  if (scena == "G05") return niskieAhi(n);
  if (scena == "G06" || scena == "G16") return solidnaNoc(n);
  if (scena == "G08") return stabilnyTydzien(n);
  if (scena == "G09") return podwyzszoneAhi(n);
  if (scena == "G10") return duzoZdarzen(n);
  if (scena == "G11" || scena == "G14") return krotkaTerapia(n);
  if (scena == "G13") return bardzoNiskieAhi(n);
  if (scena == "G15") return wszystkoWCelu(n);
  return false;
}
