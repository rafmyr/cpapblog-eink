// SPDX-License-Identifier: GPL-3.0-or-later
#include "portret_ekran.h"
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <set>
#include "kontekst_sk.h"  // sk::tekst(): teksty z zestawu danych (ADR-0023)

namespace portret {

namespace {

// atan(i / 256) w sekundach tarczy (pełny obrót = 43200 s), i = 0…256; wygenerowane: round(atan(i/256)·43200/2π)
const int16_t ATAN_TARCZY[257] = {
    0,27,54,81,107,134,161,188,215,242,268,295,322,349,376,402,429,456,483,509,536,563,589,616,643,669,696,722,
    749,776,802,829,855,881,908,934,961,987,1013,1039,1066,1092,1118,1144,1170,1196,1222,1248,1274,1300,1326,1352,
    1378,1404,1429,1455,1481,1506,1532,1557,1583,1608,1634,1659,1684,1710,1735,1760,1785,1810,1835,1860,1885,1910,
    1935,1959,1984,2009,2033,2058,2082,2107,2131,2156,2180,2204,2228,2252,2276,2300,2324,2348,2372,2396,2420,2443,
    2467,2490,2514,2537,2560,2584,2607,2630,2653,2676,2699,2722,2745,2768,2790,2813,2836,2858,2880,2903,2925,2947,
    2970,2992,3014,3036,3058,3080,3101,3123,3145,3166,3188,3209,3231,3252,3273,3294,3316,3337,3358,3378,3399,3420,
    3441,3461,3482,3502,3523,3543,3564,3584,3604,3624,3644,3664,3684,3704,3724,3743,3763,3782,3802,3821,3841,3860,
    3879,3898,3917,3936,3955,3974,3993,4012,4030,4049,4068,4086,4104,4123,4141,4159,4177,4195,4213,4231,4249,4267,
    4285,4302,4320,4338,4355,4373,4390,4407,4424,4442,4459,4476,4493,4510,4526,4543,4560,4576,4593,4610,4626,4642,
    4659,4675,4691,4707,4723,4740,4755,4771,4787,4803,4819,4834,4850,4866,4881,4896,4912,4927,4942,4957,4973,4988,
    5003,5018,5033,5047,5062,5077,5092,5106,5121,5135,5150,5164,5178,5193,5207,5221,5235,5249,5263,5277,5291,5305,
    5318,5332,5346,5359,5373,5387,5400,
};
// sin(i · 0,5°) · 65536, i = 0…180 (ćwiartka); wygenerowane: round(sin(i·0,5°)·65536)
const int32_t SIN_POL_STOPNIA[181] = {
    0,572,1144,1716,2287,2859,3430,4001,4572,5142,5712,6281,6850,7419,7987,8554,9121,9687,10252,10817,11380,11943,
    12505,13066,13626,14185,14742,15299,15855,16409,16962,17514,18064,18613,19161,19707,20252,20795,21336,21876,
    22415,22951,23486,24019,24550,25080,25607,26132,26656,27177,27697,28214,28729,29242,29753,30261,30767,31271,
    31772,32271,32768,33262,33754,34242,34729,35212,35693,36172,36647,37120,37590,38057,38521,38982,39441,39896,
    40348,40797,41243,41686,42126,42562,42995,43425,43852,44275,44695,45112,45525,45935,46341,46744,47143,47538,
    47930,48318,48703,49084,49461,49834,50203,50569,50931,51289,51643,51993,52339,52682,53020,53354,53684,54010,
    54332,54650,54963,55273,55578,55879,56175,56468,56756,57040,57319,57594,57865,58131,58393,58650,58903,59152,
    59396,59635,59870,60100,60326,60547,60764,60976,61183,61386,61584,61777,61966,62149,62328,62503,62672,62837,
    62997,63152,63303,63449,63589,63725,63856,63983,64104,64220,64332,64439,64540,64637,64729,64816,64898,64975,
    65048,65115,65177,65234,65287,65334,65376,65414,65446,65474,65496,65514,65526,65534,65536,
};

const int X_TARCZY = 200;
const int DOL_TRESCI = 434;
const int R_CYFR = 82, R_KRESKA_OD = 77, R_KRESKA_DO = 87, R_KROPKI = 110, R_SLAD = 126;
const int R_PIERSCIEN_OD = 118, R_PIERSCIEN_DO = 134, R_BIALY_OD = 121, R_BIALY_DO = 131, R_TESTU_OD = 96,
          R_TESTU_DO = 172;
// znacznik startu okna (zmiana 2026-11-12 (2), ADR-0040): kreska promieniowa od R_ZNACZNIK_OD do R_ZNACZNIK_DO (przed
// skalą), szerokość 2 × 2,25 px (4–5 px), w pierścieniu biała przerwa do 4,25 px od osi kreski (2 px po każdej stronie);
// grot (trójkąt) na zewnątrz pierścienia po stronie ruchu wskazówek zegara: podstawa 14 px przy zewnętrznym końcu
// kreski, czubek 15 px od osi kreski.
// Granice w ćwiartkach i połówkach piksela, żeby piksel nigdy nie leżał dokładnie na granicy (wzorzec w Pythonie liczy
// to samo w liczbach zmiennoprzecinkowych, T-TEST-415). [PRÓG] wygląd ekranu
const int R_ZNACZNIK_OD = 110, R_ZNACZNIK_DO = 150;
const int ZN_POL_SZER4 = 9, ZN_PRZERWA4 = 17, GROT_CZUBEK = 15;
const int KOL_X = 390, KOL_W = 382;
const int OGRANICZENIE_K = 7;       // [PRÓG]
const int PROG_WSPARCIA10 = 20;     // +2 cmH2O [PRÓG]
const int POL_PODSTAWY_S = 300;     // podstawa garbu 10 min (±5 min) [PRÓG]
const int NAJGESCIEJ_MIN = 60, NAJGESCIEJ_PIKOW = 3, PRZERWA_MIN = 10, MAKSIMUM_MIN = 5;  // [PRÓG]
const int BIALY_WYC_S = 60;         // biały odcinek: co najmniej 60 s próbek wycieku ≥ 15 L/min w odcinku [PRÓG]

long long dzielWDol(long long a, long long b) { return a >= 0 ? a / b : -((-a + b - 1) / b); }
int promien(int r, int skala) { return (r * skala + 50) / 100; }

// sin i cos kąta w połówkach stopnia (0…719) razy 65536
int32_t sinPol(int h) {
  h = ((h % 720) + 720) % 720;
  int q = h / 180, r = h % 180;
  int32_t v = (q & 1) ? SIN_POL_STOPNIA[180 - r] : SIN_POL_STOPNIA[r];
  return q >= 2 ? -v : v;
}
int32_t cosPol(int h) { return sinPol(h + 180); }
// punkt na promieniu r·skala/100 pod kątem h (połówki stopnia): x = cx + round(r sin), y = cy − round(r cos)
void punkt(int cx, int cy, int r, int skala, int h, int& x, int& y) {
  long long m = 100LL * 65536LL;
  x = cx + (int)dzielWDol((long long)r * skala * sinPol(h) + m / 2, m);
  y = cy - (int)dzielWDol((long long)r * skala * cosPol(h) + m / 2, m);
}

uint64_t isqrt(uint64_t v) {
  uint64_t r = 0, b = 1ULL << 62;
  while (b > v) b >>= 2;
  while (b) {
    if (v >= r + b) {
      v -= r + b;
      r = (r >> 1) + b;
    } else {
      r >>= 1;
    }
    b >>= 2;
  }
  return r;
}

bool wOknie(int od, int w) { return w <= od && od < w + OKNO_MIN; }

// odcinek [od, od + 5) ma część w sesji, czyli biały łuk wycieku jest na rysunku (ten sam warunek co rysowanie)
bool sesjaWOdcinku(const Przebieg& p, int od) {
  for (auto& s : p.sesje)
    if (s.od < od + 5 && s.do_ > od) return true;
  return false;
}
// odcinek biały, gdy liczy się do wycieku (co najmniej 30 poprawnych próbek; w pliku
// pole `wyciek95` niepuste) i ma co najmniej 60 s próbek `Leak` ≥ 15 L/min (pole `wyc_s`); rysunek, podpis
// `Wyciek przez {n} min` i legenda biorą ten sam warunek (sam P95 odcinka już nie decyduje o kolorze)
bool bialyOdcinek(const Odcinek& o) { return o.wyciek95 != BRAK && o.wycS != BRAK && o.wycS >= BIALY_WYC_S; }
// odcinek biały widoczny na tarczy: w oknie i z częścią w sesji (łuk pierścienia jest tylko w sesji)
bool bialyNaRysunku(const Przebieg& p, const Odcinek& o, int w) {
  return wOknie(o.od, w) && bialyOdcinek(o) && sesjaWOdcinku(p, o.od);
}

bool wSesji(const Przebieg& p, long u) {
  for (auto& s : p.sesje)
    if ((long)s.od * 60 <= u && u < (long)s.do_ * 60) return true;
  return false;
}

// obwiednia garbów w 1/16 px w chwili u (sekundy od 12:00): największy garb sąsiednich odcinków
int obwiednia(const std::vector<int>& h16, long u) {
  long k0 = u / 300;
  int m = 0;
  for (long k = k0 - 1; k <= k0 + 1; k++) {
    if (k < 0 || k >= (long)h16.size() || h16[(size_t)k] <= 0) continue;
    long d = u - (k * 300 + 150);
    if (d < 0) d = -d;
    if (d >= POL_PODSTAWY_S) continue;
    uint64_t h = (uint64_t)h16[(size_t)k];
    uint64_t p2 = (uint64_t)POL_PODSTAWY_S * POL_PODSTAWY_S;
    int g = (int)isqrt(h * h * (p2 - (uint64_t)(d * d)) / p2);
    m = std::max(m, g);
  }
  return m;
}

}  // namespace

bool asv(const std::string& t) { return t == "ASV" || t == "ASVAuto"; }

int minutyTerapii(const Przebieg& p) {
  int m = 0;
  for (auto& s : p.sesje) m += std::max(0, s.do_ - s.od);
  return m;
}
bool jestPortret(const Przebieg& p) { return !p.data.empty() && minutyTerapii(p) >= MIN_TERAPII; }

std::string hhmmOdPoludnia(int u) {
  int c = ((u + 720) % 1440 + 1440) % 1440;
  char b[8];
  snprintf(b, sizeof b, "%02d:%02d", c / 60, c % 60);
  return b;
}

int okno(const DanePortretu& d) {
  auto dol5 = [](long m) { return (int)dzielWDol(m, 5) * 5; };
  int w = dol5(d.zwykla - PRZED_STARTEM_MIN);
  const Przebieg& p = *d.przebieg;
  const Sesja* naj = nullptr;
  for (auto& s : p.sesje)
    if (!naj || s.do_ - s.od > naj->do_ - naj->od) naj = &s;
  // najdłuższa sesja nie mieści się w oknie w całości → okno od jej startu minus 2 h; gdy wtedy koniec wypada
  // za oknem, okno kończy się na końcu sesji; sesja dłuższa niż tarcza: okno od startu sesji
  if (naj && (naj->od < w || naj->do_ > w + OKNO_MIN)) {
    w = dol5(naj->od - PRZED_STARTEM_MIN);
    if (w + OKNO_MIN < naj->do_) w = -dol5(-(naj->do_ - OKNO_MIN));  // w górę do 5 min
    if (w > naj->od) w = dol5(naj->od);
  }
  return w;
}

int katTarczy(int dx, int dy) {
  long X = dx, Y = -dy;  // Y na północ
  long u = X < 0 ? -X : X, v = Y < 0 ? -Y : Y;
  if (u == 0 && v == 0) return 0;
  auto at = [](long a, long b) {  // atan(a / b) dla 0 ≤ a ≤ b, w sekundach tarczy
    if (b <= 0) return 0L;        // nieosiągalne: u, v nie są oba zerem
    long t = (a << 16) / b;       // 0…65536
    long i = t >> 8, f = t & 255;
    if (i >= 256) return (long)ATAN_TARCZY[256];
    return (long)ATAN_TARCZY[i] + (((long)ATAN_TARCZY[i + 1] - ATAN_TARCZY[i]) * f + 128) / 256;
  };
  long ne = u <= v ? at(u, v) : 10800 - at(v, u);  // kąt od północy w ćwiartce
  long k;
  if (X >= 0 && Y >= 0) k = ne;
  else if (X >= 0) k = 21600 - ne;
  else if (Y < 0) k = 21600 + ne;
  else k = 43200 - ne;
  return (int)(k % 43200);
}

int pikZewn16(const DanePortretu& d, const Odcinek& o, int skala) {
  if (asv(d.tryb)) {
    if (d.normaPs10 == BRAK || o.ps == BRAK || d.przebieg->brak('P')) return 0;
    int d10 = o.ps - d.normaPs10;
    if (d10 < PROG_WSPARCIA10) return 0;
    long f = std::min(4000L, 1800L + 55L * (d10 - PROG_WSPARCIA10));  // (0,45 + 0,55·(ΔPS − 2)/4) · 4000
    return (int)(16L * 36L * f * skala / (4000L * 100L));
  }
  if (o.zew == BRAK || o.zew <= 0) return 0;
  long k = std::min(o.zew, OGRANICZENIE_K);
  long h = 16L * 36L * (210L + 65L * (k - 1)) * skala / (600L * 100L);  // 36 · (0,35 + 0,65·(k − 1)/6)
  return (int)std::max(h, 16L * 12L * skala / 100L);
}

int pikWewn16(const DanePortretu& d, const Odcinek& o, int skala) {
  if (asv(d.tryb) || o.centr == BRAK || o.centr <= 0) return 0;
  long k = std::min(o.centr, OGRANICZENIE_K);
  return (int)(16L * 20L * (270L + 55L * (k - 1)) * skala / (600L * 100L));  // 20 · (0,45 + 0,55·(k − 1)/6)
}

std::string pierwszyPodpis(const DanePortretu& d) {
  // (decyzja właściciela projektu 11.11.2026, wariant (b), R-POR-25): po nocy niepewnej zamiast zdania o tygodniu opis tej nocy
  return d.niepewna ? sk::tekst("S11.noc_niepewna") : d.pierwszy;
}

std::vector<std::pair<std::string, std::string>> braki(const DanePortretu& d) {
  const Przebieg& pr = *d.przebieg;
  std::vector<std::pair<std::string, std::string>> b;  // (podpis „B”, krótka nazwa do podpisu zbiorczego)
  if (asv(d.tryb)) {
    if (pr.brak('P')) b.push_back({sk::tekst("S11.ps_brak"), sk::tekst("S11.nazwa_wsparcie")});
    else if (d.normaPs10 == BRAK) b.push_back({sk::tekst("S11.ps_uczy"), sk::tekst("S11.nazwa_norma_ps")});
  }
  if (pr.brak('L')) b.push_back({sk::tekst("S11.wyciek_brak"), sk::tekst("S11.nazwa_wyciek")});
  if (pr.brak('E')) b.push_back({sk::tekst("S11.zdarzenia_brak"), sk::tekst("S11.nazwa_zdarzenia")});
  if (!asv(d.tryb) && pr.brak('R')) b.push_back({sk::tekst("S11.rodzaj_brak"), sk::tekst("S11.nazwa_rodzaj")});
  if (pr.brak('M') && (d.tryb == "AutoSet" || d.tryb == "ASVAuto"))  // maksimum z ustawień nieznane
    b.push_back({sk::tekst(d.tryb == "AutoSet" ? "S11.cisnienie_maks_brak" : "S11.epap_maks_brak"),
                 sk::tekst("S11.nazwa_maksimum")});
  return b;
}

std::vector<std::string> podpisyBrakow(const DanePortretu& d, size_t miejsc) {
  std::vector<std::pair<std::string, std::string>> b = braki(d);
  std::vector<std::string> w;
  if (b.size() <= miejsc) {
    for (auto& x : b) w.push_back(x.first);
    return w;
  }
  // więcej braków niż miejsc: pierwsze (miejsc − 1) pełnym podpisem, reszta w jednym podpisie zbiorczym z nazwami
  // (żaden brak nie wypada); podpis zbiorczy zawija się w najwyżej 2 linie kolumny (rysujKolumne())
  for (size_t i = 0; i + 1 < miejsc; i++) w.push_back(b[i].first);
  std::string lista;
  for (size_t i = miejsc ? miejsc - 1 : 0; i < b.size(); i++) lista += (lista.empty() ? "" : ", ") + b[i].second;
  w.push_back(sk::tekst("S11.braki", {{"lista", lista}}));
  return w;
}

std::vector<std::string> podpisy(const DanePortretu& d, bool linia) {
  const Przebieg& pr = *d.przebieg;
  const int w = okno(d);
  const size_t limit = linia ? 2 : 4;
  std::vector<std::string> p = {pierwszyPodpis(d)};
  for (auto& b : podpisyBrakow(d, limit - 1)) p.push_back(b);
  std::vector<int> piki;
  for (auto& o : pr.odcinki)
    if (wOknie(o.od, w) && pikZewn16(d, o) > 0) piki.push_back(o.od);
  int najl = 0, najs = 0;
  for (int j = w; j < w + OKNO_MIN - NAJGESCIEJ_MIN + 5; j += 5) {
    int n = 0;
    for (int od : piki) n += j <= od && od < j + NAJGESCIEJ_MIN;
    if (n > najl) {
      najl = n;
      najs = j;
    }
  }
  if (najl >= NAJGESCIEJ_PIKOW) p.push_back(sk::tekst("S11.najgesciej", {{"od", hhmmOdPoludnia(najs)}, {"do", hhmmOdPoludnia(najs + NAJGESCIEJ_MIN)}}));
  if (!pr.brak('L')) {
    long s = 0;  // tylko odcinki białe na rysunku (ten sam warunek co rysowanie i legenda)
    for (auto& o : pr.odcinki)
      if (bialyNaRysunku(pr, o, w)) s += o.wycS;
    long m = (s + 30) / 60;
    if (m >= 1) p.push_back(sk::tekst("S11.wyciek", {{"n", std::to_string(m)}}));
  }
  std::vector<Sesja> ses;
  for (auto& s : pr.sesje)
    if (s.do_ > w && s.od < w + OKNO_MIN) ses.push_back({std::max(s.od, w), std::min(s.do_, w + OKNO_MIN)});
  std::sort(ses.begin(), ses.end(), [](const Sesja& a, const Sesja& b) { return a.od != b.od ? a.od < b.od : a.do_ < b.do_; });
  std::vector<Sesja> przerwy;
  for (size_t i = 0; i + 1 < ses.size(); i++)
    if (ses[i + 1].od - ses[i].do_ >= PRZERWA_MIN) przerwy.push_back({ses[i].do_, ses[i + 1].od});
  if (przerwy.size() == 1) {
    p.push_back(sk::tekst("S11.przerwa", {{"od", hhmmOdPoludnia(przerwy[0].od)}, {"do", hhmmOdPoludnia(przerwy[0].do_)}}));
  } else if (!przerwy.empty()) {
    int mx = 0;
    for (auto& x : przerwy) mx = std::max(mx, x.do_ - x.od);
    p.push_back(sk::tekst("S11.przerwy", {{"n", std::to_string(przerwy.size())}, {"m", std::to_string(mx)}}));
  }
  if (!asv(d.tryb) && !pr.brak('E')) {
    int c = 0;  // z odcinków (wszystkie zdarzenia), nie z wierszy `Z` (najwyżej 500)
    for (auto& o : pr.odcinki)
      if (wOknie(o.od, w) && o.centr != BRAK) c += o.centr;
    if (c >= 1) p.push_back(sk::tekst("S11.centralne", {{"n", std::to_string(c)}}));
  }
  if (d.tryb == "AutoSet" || d.tryb == "ASVAuto") {  // decyzja właściciela projektu 11.11.2026 (AutoSet i ASVAuto)
    int mk = 0;
    for (auto& o : pr.odcinki)
      if (wOknie(o.od, w) && o.maks != BRAK) mk += o.maks;
    if (mk >= MAKSIMUM_MIN)
      p.push_back(sk::tekst(d.tryb == "AutoSet" ? "S11.cisnienie_maks" : "S11.epap_maks", {{"n", std::to_string(mk)}}));
  }
  bool poza = false, czesc = false;
  for (auto& s : pr.sesje) {
    bool cala = s.do_ <= w || s.od >= w + OKNO_MIN;
    poza = poza || cala;
    czesc = czesc || (!cala && (s.od < w || s.do_ > w + OKNO_MIN));
  }
  if (czesc) p.push_back(sk::tekst("S11.czesc_nocy"));  // część nocy przed drzemką, oba, gdy jest miejsce
  if (poza) p.push_back(sk::tekst("S11.drzemka"));
  p.resize(std::min(p.size(), limit));
  return p;
}

std::vector<std::string> legenda(const DanePortretu& d) {
  const Przebieg& pr = *d.przebieg;
  const int w = okno(d);
  std::vector<std::string> l = {"maska"};
  bool wyc = false, zew = false, wew = false, krop = false;
  for (auto& o : pr.odcinki) {
    if (!wOknie(o.od, w)) continue;
    wyc = wyc || bialyNaRysunku(pr, o, w);
    zew = zew || pikZewn16(d, o) > 0;
    wew = wew || pikWewn16(d, o) > 0;
  }
  for (auto& z : pr.zdarzenia) krop = krop || wOknie(z.minuta, w);
  if (wyc) l.push_back("wyciek");
  if (zew) l.push_back(asv(d.tryb) ? "wsparcie" : "zdarzenia");
  if (wew) l.push_back("centralne");
  if (asv(d.tryb) && krop) l.push_back("kropka");
  return l;
}

std::string opisLegendy(const std::string& k) { return sk::tekst("S11.leg_" + k); }

}  // namespace portret

using namespace portret;

namespace {

// R-POR-06 (zmiana 2026-11-12 (2)): znacznik startu okna tarczy pod kątem minuty `w` (minuty od 12:00; minuta tarczy
// to pół stopnia). Przyjmuje środek tarczy, początek okna i skalę w setnych; rysuje kreskę przez tor pierścienia
// (czarna, z białą przerwą po obu stronach w obrębie pierścienia, żeby była widoczna na czarnym łuku maski) i grot
// wskazujący kierunek ruchu wskazówek zegara. Bez liczb i bez napisu. Test przynależności piksela w układzie
// „wzdłuż promienia / w poprzek” z tablicą sinusów, arytmetyka 64-bitowa (iloczyny do ok. 2·10^8).
// (12.11.2026): `bezToru` = pierwszy albo ostatni odcinek okna jest biały na rysunku (wyciek
// przy starcie okna); wtedy kreska i biała przerwa nie wchodzą w tor pierścienia (promienie rIn…rOut), żeby biały
// odcinek z czarnym obrysem został nietknięty: znacznik to część kreski pod pierścieniem, nad nim i grot.
void rysujZnacznikStartu(Plotno& p, int cx, int cy, int w, int skala, bool bezToru) {
  const int h = ((w % 720) + 720) % 720;
  const int64_t J = 65536, s = sinPol(h), c = cosPol(h);
  const int64_t r1 = promien(R_ZNACZNIK_OD, skala), r2 = promien(R_ZNACZNIK_DO, skala);
  const int64_t rIn = promien(R_PIERSCIEN_OD, skala), rOut = promien(R_PIERSCIEN_DO, skala);
  const int z = (int)r2 + GROT_CZUBEK + 2;
  for (int y = cy - z; y <= cy + z; y++)
    for (int x = cx - z; x <= cx + z; x++) {
      const int64_t X = x - cx, Y = cy - y;
      const int64_t a = X * s + Y * c;  // wzdłuż promienia znacznika, ×65536
      const int64_t t = X * c - Y * s;  // w poprzek, dodatnie w kierunku ruchu wskazówek zegara, ×65536
      const int64_t t4 = 4 * (t < 0 ? -t : t);
      if (2 * a >= (2 * r1 - 1) * J && 2 * a < (2 * r2 - 1) * J) {  // a ∈ [r1 − ½, r2 − ½)
        const int64_t rr = X * X + Y * Y;
        const bool tor = rr >= rIn * rIn && rr < rOut * rOut;
        if (bezToru && tor) continue;
        if (t4 < ZN_POL_SZER4 * J) {
          p.piksel(x, y, true);
          continue;
        }
        if (t4 < ZN_PRZERWA4 * J && tor) {
          p.piksel(x, y, false);
          continue;
        }
      }
      // grot: t ∈ [2,25; 15), |a − (r2 − 7,5)| ≤ 28/51 · (15 − t) (podstawa 14 px przy t = 2,25)
      if (4 * t >= ZN_POL_SZER4 * J && t < GROT_CZUBEK * J) {
        int64_t da = 2 * a - (2 * r2 - 15) * J;
        if (da < 0) da = -da;
        if (51 * da <= 56 * (GROT_CZUBEK * J - t)) p.piksel(x, y, true);
      }
    }
}

void rysujTarcze(Plotno& p, const DanePortretu& d, int cy, int skala) {
  const Przebieg& pr = *d.przebieg;
  const int w = okno(d), cx = X_TARCZY;
  const int rIn = promien(R_PIERSCIEN_OD, skala), rOut = promien(R_PIERSCIEN_DO, skala);
  const int bIn = promien(R_BIALY_OD, skala), bOut = promien(R_BIALY_DO, skala);
  const int rMin = promien(R_TESTU_OD, skala), rMax = promien(R_TESTU_DO, skala);
  std::vector<int> hz((size_t)ODCINKI_DOBY, 0), hw((size_t)ODCINKI_DOBY, 0);
  std::vector<bool> biale((size_t)ODCINKI_DOBY, false);
  for (auto& o : pr.odcinki) {
    size_t k = (size_t)(o.od / 5);
    if (wOknie(o.od, w)) {
      hz[k] = pikZewn16(d, o, skala);
      hw[k] = pikWewn16(d, o, skala);
    }
    biale[k] = bialyOdcinek(o);
  }
  // 1. ślad tarczy: kropki 2×2 co 2°
  for (int j = 0; j < 180; j++) {
    int x, y;
    punkt(cx, cy, R_SLAD, skala, 4 * j, x, y);
    p.prostokat(x - 1, y - 1, 2, 2, true);
  }
  // 2.–5. pierścień (maska), wyciek, piki na zewnątrz i do środka: test biegunowy w pierścieniu rMin…rMax
  const long baza = (long)w * 60;
  const long bazaTarczy = ((baza % 43200) + 43200) % 43200;
  for (int y = cy - rMax; y <= cy + rMax; y++)
    for (int x = cx - rMax; x <= cx + rMax; x++) {
      long dx = x - cx, dy = y - cy, r2 = dx * dx + dy * dy;
      if (r2 < (long)rMin * rMin || r2 >= (long)rMax * rMax) continue;
      long q = katTarczy((int)dx, (int)dy);
      long u = baza + ((q - bazaTarczy) % 43200 + 43200) % 43200;
      if (r2 >= (long)rIn * rIn && r2 < (long)rOut * rOut && wSesji(pr, u)) {
        bool bialy = r2 >= (long)bIn * bIn && r2 < (long)bOut * bOut && u >= 0 && u / 300 < ODCINKI_DOBY &&
                     biale[(size_t)(u / 300)];
        p.piksel(x, y, !bialy);
      }
      if (r2 >= (long)rOut * rOut) {
        long g = obwiednia(hz, u);
        long gr = (long)rOut * 16 + g;
        if (g > 0 && r2 * 256 < gr * gr) p.piksel(x, y, true);
      }
      if (r2 < (long)rIn * rIn) {
        long g = obwiednia(hw, u);
        long gr = (long)rIn * 16 - g;
        if (g > 0 && gr > 0 && r2 * 256 >= gr * gr) p.piksel(x, y, true);
      }
    }
  // 6. znacznik startu okna (nad pierścieniem i pikami, pod kropkami ASV i cyframi)
  // wyciek w pierwszym albo ostatnim odcinku okna → znacznik omija tor pierścienia (przerwa sięga najwyżej
  // 4,25 px, czyli mniej niż 5 min łuku także w skali 0,85, więc dotyczy tylko tych dwóch odcinków)
  bool wyciekPrzyStarcie = false;
  for (int od : {w, w + OKNO_MIN - 5}) {
    const Odcinek* o = od >= 0 ? pr.odcinek(od / 5) : nullptr;
    wyciekPrzyStarcie = wyciekPrzyStarcie || (o && bialyNaRysunku(pr, *o, w));
  }
  rysujZnacznikStartu(p, cx, cy, w, skala, wyciekPrzyStarcie);
  // 7. kropki zdarzeń ASV (koło 8 px po wewnętrznej stronie pierścienia)
  if (asv(d.tryb))
    for (auto& z : pr.zdarzenia) {
      if (!wOknie(z.minuta, w)) continue;
      int x, y;
      punkt(cx, cy, R_KROPKI, skala, ((z.minuta % 720) + 720) % 720, x, y);
      p.kolo(x, y, 4, true);
    }
  // 8. kreski godzin i cyfry; środek tarczy pusty (od 12.11.2026 bez napisu okna, R-POR-06)
  for (int g : {1, 2, 4, 5, 7, 8, 10, 11}) {
    int x1, y1, x2, y2;
    punkt(cx, cy, R_KRESKA_OD, skala, 60 * g, x1, y1);
    punkt(cx, cy, R_KRESKA_DO, skala, 60 * g, x2, y2);
    p.linia(x1, y1, x2, y2);
    int ax = x2 > x1 ? x2 - x1 : x1 - x2, ay = y2 > y1 ? y2 - y1 : y1 - y2;
    if (ay > ax) p.linia(x1 + 1, y1, x2 + 1, y2);
    else p.linia(x1, y1 + 1, x2, y2 + 1);
  }
  const GFXglyph* h24 = &F_P24B->glyph['H' - F_P24B->first];
  const int cap24 = h24->height;
  const char* cyfry[4] = {"12", "3", "6", "9"};
  for (int i = 0; i < 4; i++) {
    int x, y;
    punkt(cx, cy, R_CYFR, skala, 180 * i, x, y);
    tekst(p, F_P24B, x - szer(F_P24B, cyfry[i]) / 2, y + cap24 / 2, cyfry[i]);
  }
}

void znakLegendy(Plotno& p, const std::string& k, int x, int y) {
  if (k == "maska") {
    p.prostokat(x, y + 6, 30, 8, true);
  } else if (k == "wyciek") {
    p.prostokat(x, y + 6, 30, 8, true);
    p.prostokat(x + 3, y + 8, 24, 4, false);
  } else if (k == "wsparcie" || k == "zdarzenia") {
    p.prostokat(x, y + 14, 30, 6, true);
    p.trojkat(x + 7, y + 14, x + 15, y, x + 23, y + 14);
  } else if (k == "centralne") {
    p.prostokat(x, y, 30, 6, true);
    p.trojkat(x + 7, y + 6, x + 15, y + 20, x + 23, y + 6);
  } else if (k == "kropka") {
    p.prostokat(x, y + 2, 30, 6, true);
    p.kolo(x + 15, y + 15, 4, true);
  }
}

void rysujKolumne(Plotno& p, const DanePortretu& d, int gora, bool linia) {
  int y = gora + 8;
  // po nocy niepewnej sam czas z maską, bez AHI i bez drugiego wiersza; o nocy
  // niepewnej mówi jeden komunikat, pierwszy podpis `Tej nocy nie oceniam: duży wyciek.` (R-POR-15, R-POR-25)
  if (d.niepewna) tekst(p, F_P28B, KOL_X, y + 26, sk::tekst("S11.liczby_czas", {{"czas", d.czas}}));
  else tekst(p, F_P28B, KOL_X, y + 26, sk::tekst("S11.liczby", {{"czas", d.czas}, {"x", d.ahi.empty() ? std::string("–") : d.ahi}}));
  y += 34;
  y += 10;
  std::vector<std::string> pd = podpisy(d, linia);
  for (size_t i = 0; i < pd.size(); i++) {
    if (i == 0) {
      std::vector<std::string> l = zawin(F_P28B, pd[0], KOL_W);
      if (l.size() > 2) l.resize(2);
      for (auto& s : l) {
        tekst(p, F_P28B, KOL_X, y + 26, s);
        y += 34;
      }
    } else {  // podpisy 2–7 i „B” w 1 linii; podpis zbiorczy braków najwyżej w 2 liniach (R-POR-27)
      std::vector<std::string> l = zawin(F_P24B, pd[i], KOL_W);
      if (l.size() > 2) l.resize(2);
      for (auto& s : l) {
        tekst(p, F_P24B, KOL_X, y + 24, s);
        y += 32;
      }
    }
  }
  y += 8;
  p.prostokat(KOL_X, y, KOL_W, 1, true);
  y += 9;
  for (auto& k : legenda(d)) {
    znakLegendy(p, k, KOL_X, y + 4);
    tekst(p, F_P22B, KOL_X + 42, y + 21, opisLegendy(k));
    y += 28;
  }
}

}  // namespace

void rysujCialoPortretu(Plotno& p, const DanePortretu& d, int gora, bool linia) {
  if (!d.przebieg) return;
  int skala = linia ? SKALA_LINII : 100;
  rysujTarcze(p, d, (gora + DOL_TRESCI) / 2, skala);
  rysujKolumne(p, d, gora, linia);
}
