// SPDX-License-Identifier: GPL-3.0-or-later
#include "serwer_raportu.h"

#include "wspolne.h"

namespace serwer {
namespace {
GniazdoHttp* g_gniazdo = nullptr;

std::string html(const std::string& s) {  // ucieczka znaków HTML w treści strony
  std::string r;
  for (char c : s) {
    if (c == '<') r += "&lt;";
    else if (c == '>') r += "&gt;";
    else if (c == '&') r += "&amp;";
    else if (c == '"') r += "&quot;";
    else r += c;
  }
  return r;
}
std::string nazwaZSciezki(const std::string& s) {
  size_t p = s.rfind('/');
  return p == std::string::npos ? s : s.substr(p + 1);
}
}  // namespace

GniazdoHttp* gniazdo() { return g_gniazdo; }
void ustawGniazdo(GniazdoHttp* g) { g_gniazdo = g; }

std::string token(const uint8_t b[BAJTY_TOKENU]) {
  static const char A[] = "0123456789abcdefghjkmnpqrstvwxyz";
  std::string r;
  uint32_t bufor = 0;
  int bity = 0;
  for (int i = 0; i < BAJTY_TOKENU; i++) {
    bufor = (bufor << 8) | b[i];
    bity += 8;
    while (bity >= 5) {
      r += A[(bufor >> (bity - 5)) & 31];
      bity -= 5;
    }
  }
  if (bity > 0) r += A[(bufor << (5 - bity)) & 31];
  return r;
}

bool adresPrywatny(const std::string& ip) {
  int o[4] = {0, 0, 0, 0}, i = 0, cyfr = 0;
  for (char c : ip) {
    if (c == '.') {
      if (cyfr == 0 || ++i > 3) return false;
      cyfr = 0;
    } else if (c >= '0' && c <= '9') {
      o[i] = o[i] * 10 + (c - '0');
      if (++cyfr > 3 || o[i] > 255) return false;
    } else {
      return false;
    }
  }
  if (i != 3 || cyfr == 0) return false;
  return o[0] == 10 || (o[0] == 172 && o[1] >= 16 && o[1] <= 31) || (o[0] == 192 && o[1] == 168) ||
         (o[0] == 169 && o[1] == 254);
}

bool rowneStalyCzas(const std::string& a, const std::string& b) {
  unsigned r = a.size() == b.size() ? 0u : 1u;
  for (size_t i = 0; i < a.size(); i++) r |= (unsigned)(unsigned char)a[i] ^ (unsigned)(unsigned char)(i < b.size() ? b[i] : 0);
  return r == 0;
}

std::vector<Pozycja> pozycjeZPlikow(const std::vector<std::string>& sciezki) {
  std::vector<Pozycja> r;
  for (auto& s : sciezki) {
    std::string n = nazwaZSciezki(s);
    if (n.size() < 15 || n.compare(n.size() - 4, 4, ".pdf") != 0 || !dataOk(n.substr(0, 10))) continue;
    r.push_back({s.compare(0, 9, "/raporty/") == 0 ? s : "/raporty/" + n, n.substr(0, 10)});
  }
  for (size_t i = 1; i < r.size(); i++)  // od najnowszej (data, potem nazwa)
    for (size_t j = i; j > 0 && (r[j].data > r[j - 1].data ||
                                 (r[j].data == r[j - 1].data && r[j].sciezka > r[j - 1].sciezka));
         j--)
      std::swap(r[j], r[j - 1]);
  return r;
}

void Sesja::otworz(const std::string& t, uint32_t ms, const std::string& adres, Tryb tryb, const std::string& biezacy,
                   const std::vector<Pozycja>& raporty, const std::string& dzis, const std::string& temat,
                   const std::string& okres, bool edfDostepny) {
  otwarta_ = true;
  starsze_ = false;
  zle_ = 0;
  od_ = ms;
  token_ = t;
  adres_ = adres;
  tryb_ = tryb;
  biezacy_ = biezacy;
  dzis_ = dzis;
  temat_ = temat;
  okres_ = okres;
  edf_ = edfDostepny;
  wszystkie_ = raporty;
  liczWidoczne();
}

bool Sesja::aktywna(uint32_t ms) const { return otwarta_ && zle_ < ZLE_MAKS && ms - od_ < CZAS_MS; }

void Sesja::liczWidoczne() {
  widoczne_.clear();
  std::string granica = dodajDni(dzis_, -(DNI_LISTY - 1));
  for (auto& p : wszystkie_)
    if (starsze_ || p.data >= granica) widoczne_.push_back(p);
}

std::string Sesja::adresQr() const {
  return "http://" + adres_ + "/r/" + token_ + (tryb_ == Tryb::SCIAGA ? "/edf" : "/");
}
std::string Sesja::adresReczny() const { return adres_ + "/r/" + token_; }

std::string Sesja::strona() const {
  std::string b = "/r/" + token_ + "/";
  std::string s =
      "<!doctype html><html lang=\"pl\"><head><meta charset=\"utf-8\"><meta name=\"viewport\" "
      "content=\"width=device-width,initial-scale=1\"><meta name=\"color-scheme\" content=\"light\"><title>Raport "
      "dla lekarza</title><style>:root{color-scheme:light}body{font-family:"
      "sans-serif;max-width:34em;margin:1em auto;padding:0 1em;color:#000;background:#fff}a.p,button{display:block;"
      "width:100%;box-sizing:border-box;margin:.6em 0;padding:.9em;font-size:1.1em;text-align:center;border:2px solid "
      "#000;border-radius:6px;-webkit-appearance:none;appearance:none;background:#000;color:#fff;text-decoration:none;"
      "font-weight:bold}li{margin:.4em 0}</style></head><body>"
      "<link rel=\"icon\" href=\"data:,\"><h1>Raport dla lekarza</h1>";
  s += "<p>" + html(temat_) + (okres_.empty() ? "" : " · " + html(okres_)) + "</p>";
  if (!biezacy_.empty()) {
    std::string n = nazwaZSciezki(biezacy_);
    s += "<a class=\"p\" href=\"" + b + "raport.pdf\" download=\"" + html(n) + "\">Pobierz PDF</a>";
    s += "<button id=\"u\">Udostępnij</button>";
    s += "<p>Wysyłając PDF mailem, podaj w wiadomości imię, nazwisko i datę urodzenia.</p>";
    s += "<script>document.getElementById('u').onclick=async function(){try{var r=await fetch('" + b +
         "raport.pdf');var f=new File([await r.blob()],'" + html(n) +
         "',{type:'application/pdf'});if(navigator.canShare&&navigator.canShare({files:[f]})){await navigator.share({"
         "files:[f],title:'Raport dla lekarza'});return;}}catch(e){}location.href='" + b + "raport.pdf';};</script>";
  }
  s += "<p><a href=\"" + b + "edf\">Paczka EDF dla lekarza (OSCAR/SleepHQ)</a>" +
       std::string(edf_ ? "" : " · AirBridge nie odpowiada") + "</p>";
  s += "<h2>Poprzednie raporty</h2><ul>";
  for (auto& p : widoczne_) {
    std::string n = nazwaZSciezki(p.sciezka);
    s += "<li><a href=\"" + b + "p/" + html(n) + "\">" + html(dataZRokiem(p.data)) + " · " + html(n) + "</a></li>";
  }
  if (widoczne_.empty()) s += "<li>–</li>";
  s += "</ul></body></html>";
  return s;
}

Odpowiedz Sesja::obsluz(const std::string& sciezkaPelna, uint32_t ms) {
  Odpowiedz o;
  o.cialo = "Nie ma takiej strony.";
  if (!aktywna(ms)) return o;  // serwer zamknięty: gniazdo i tak nie powinno już słuchać
  std::string s = sciezkaPelna.substr(0, sciezkaPelna.find('?'));
  // ikony, o które przeglądarka pyta sama (bez tokenu), nie liczą się do złych żądań
  if (s == "/favicon.ico" || s.compare(0, 17, "/apple-touch-icon") == 0) return o;
  std::string b = "/r/" + token_;
  size_t k = s.find('/', 3);
  std::string tk = s.compare(0, 3, "/r/") == 0 ? s.substr(3, k == std::string::npos ? std::string::npos : k - 3) : "";
  bool ok = !token_.empty() && rowneStalyCzas(tk, token_);
  if (!ok) {  // R-LEK-03, R-LEK-03b: bez ważnego tokenu 404 i licznik
    zle_++;
    return o;
  }
  std::string r = s.substr(b.size());
  if (r.empty() || r == "/") {
    if (tryb_ == Tryb::SCIAGA) {
      o.edf = true;
    } else {
      o.kod = 200;
      o.typ = "text/html; charset=utf-8";
      o.cialo = strona();
    }
    return o;
  }
  if (r == "/raport.pdf" && !biezacy_.empty()) {
    o.kod = 200;
    o.typ = "application/pdf";
    o.cialo.clear();
    o.plik = biezacy_;
    o.nazwaPobrania = nazwaZSciezki(biezacy_);
    o.pdfPobrany = true;
    return o;
  }
  if (r == "/edf") {
    o.edf = true;
    return o;
  }
  if (r.compare(0, 3, "/p/") == 0) {  // R-LEK-32: tylko pozycje z widocznej listy
    std::string n = r.substr(3);
    for (auto& p : widoczne_)
      if (nazwaZSciezki(p.sciezka) == n) {
        o.kod = 200;
        o.typ = "application/pdf";
        o.cialo.clear();
        o.plik = p.sciezka;
        o.nazwaPobrania = n;
        return o;
      }
    return o;  // z ważnym tokenem 404 bez licznika złych żądań
  }
  return o;
}

}  // namespace serwer
