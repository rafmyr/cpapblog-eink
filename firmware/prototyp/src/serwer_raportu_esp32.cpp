// SPDX-License-Identifier: GPL-3.0-or-later
// Gniazdo HTTP serwera raportu i formularza konfiguracji na panelu (adapter T-KOMP-16 do src/serwer_raportu.h), WebServer
// z rdzenia Arduino-ESP32 na porcie 80 w sieci domowej. Tylko GET; każda ścieżka idzie do rdzenia (Sesja::obsluz()),
// który odpowiada 404 bez ważnego tokenu. Nagłówki odpowiedzi bez adresu i nazwy AirBridge (R-LEK-20).
// Rejestracja przy starcie programu (obiekt statyczny), więc src/main.cpp jej nie tworzy.
#if defined(ARDUINO) || defined(ARDUINO_ATRAPA)
#include <WebServer.h>

#include <new>

#include "serwer_raportu.h"

namespace {

class GniazdoEsp32 : public serwer::GniazdoHttp {
 public:
  bool start(uint16_t port) override {
    stop();
    s_ = new (std::nothrow) WebServer(port);
    if (!s_) return false;
    s_->onNotFound([this]() { jedno(); });
    s_->begin();
    return true;
  }
  void stop() override {
    if (!s_) return;
    s_->stop();
    delete s_;
    s_ = nullptr;
  }
  void obsluz(const std::function<serwer::Odpowiedz(const std::string&)>& f) override {
    if (!s_) return;
    f_ = &f;
    s_->handleClient();
    f_ = nullptr;
  }
  // formularz konfiguracji (GET strona, POST zapis). Ciało POST składane z argumentów WebServer (bez `plain`),
  // najwyżej 4 KB [PRÓG]; hasła z ciała nie trafiają do logu (adapter niczego nie wypisuje).
  void obsluzZadania(const std::function<serwer::Odpowiedz(const Zadanie&)>& f) override {
    if (!s_) return;
    fz_ = &f;
    s_->handleClient();
    fz_ = nullptr;
  }

 private:
  WebServer* s_ = nullptr;
  const std::function<serwer::Odpowiedz(const std::string&)>* f_ = nullptr;
  const std::function<serwer::Odpowiedz(const Zadanie&)>* fz_ = nullptr;
  static std::string koduj(const String& s) {
    static const char H[] = "0123456789ABCDEF";
    std::string r;
    const char* c = s.c_str();
    for (size_t i = 0; i < (size_t)s.length(); i++) {
      unsigned char u = (unsigned char)c[i];
      if ((u >= 'a' && u <= 'z') || (u >= 'A' && u <= 'Z') || (u >= '0' && u <= '9') || u == '-' || u == '_' || u == '.')
        r += (char)u;
      else {
        r += '%';
        r += H[u >> 4];
        r += H[u & 15];
      }
    }
    return r;
  }
  void jedno() {
    serwer::Odpowiedz o;
    if (fz_) {
      Zadanie z;
      z.metoda = s_->method() == HTTP_GET ? "GET" : s_->method() == HTTP_POST ? "POST" : "INNA";
      z.sciezka = s_->uri().c_str();
      if (s_->method() == HTTP_POST)
        for (int i = 0; i < s_->args() && z.cialo.size() <= 4096; i++) {
          if (s_->argName(i) == "plain") continue;
          if (!z.cialo.empty()) z.cialo += '&';
          z.cialo += koduj(s_->argName(i)) + "=" + koduj(s_->arg(i));
        }
      // ciało nierozłożone na argumenty (inny Content-Type): surowe `plain`, rdzeń sam je rozkłada
      if (s_->method() == HTTP_POST && z.cialo.empty() && s_->hasArg("plain")) z.cialo = s_->arg("plain").c_str();
      o = (*fz_)(z);
    } else if (f_ && s_->method() == HTTP_GET) o = (*f_)(std::string(s_->uri().c_str()));
    if (!o.nazwaPobrania.empty())
      s_->sendHeader("Content-Disposition", String(("inline; filename=\"" + o.nazwaPobrania + "\"").c_str()));
    s_->sendHeader("Cache-Control", "no-store");
    s_->setContentLength(o.cialo.size());
    s_->send(o.kod, o.typ.c_str(), "");
    if (!o.cialo.empty()) s_->sendContent(o.cialo.data(), o.cialo.size());
  }
};

GniazdoEsp32 g_esp32;
struct Rejestracja {
  Rejestracja() { serwer::ustawGniazdo(&g_esp32); }
} g_rejestracja;

}  // namespace
#endif
