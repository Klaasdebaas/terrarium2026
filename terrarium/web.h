#pragma once
#include "config.h"
#include "control.h"
#include "state.h"

struct WebContext {
  Config* config; Overrides* overrides; const SystemState* state;
  void (*onSmsTest)(); void (*onConfigChanged)();
};

// WebServer op poort 80. In AP-modus (lege ssid) alleen GET / (setupformulier) en POST /api/setup.
void webBegin(WebContext& ctx);
void webUpdate();   // server.handleClient(); voert ook de uitgestelde herstart uit
