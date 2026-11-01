#ifndef TERRARIUM_WEB_H
#define TERRARIUM_WEB_H
#include "config.h"
#include "control.h"
#include "state.h"

struct WebContext {
  Config* config; Overrides* overrides; const SystemState* state;
  void (*onSmsTest)(); void (*onConfigChanged)();
  void (*onConfigRejected)(uint8_t n, const char* err);   // POST /api/config met geweigerde waarden
  void (*onRelayCommand)(RelayId r, RelayMode m);          // handbediening via POST /api/relay
};

// WebServer op poort 80. In AP-modus (lege ssid) alleen GET / (setupformulier) en POST /api/setup.
void webBegin(WebContext& ctx);
void webUpdate();   // server.handleClient(); voert ook de uitgestelde herstart uit

#endif  // TERRARIUM_WEB_H
