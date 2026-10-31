#pragma once
// Pure JSON-opmaak en -parsing voor de web-API. Geen hardware.
#include <stddef.h>
#include "state.h"

// Schrijft de status-JSON (spec 6.2, relais met "manual_remaining_s") in buf.
// Geeft de lengte terug, of 0 (en buf = "") als buf te klein is.
size_t buildStatusJson(const SystemState& s, char* buf, size_t len);
// {"relay":1..4,"mode":"on"|"off"|"auto"}
bool parseRelayCommand(const char* json, RelayId& r, RelayMode& m);
// "YYYY-MM-DDTHH:MM:SS" (lokale tijd), met geldige datum en tijd.
bool parseIsoLocal(const char* s, LocalTime& t);
// {"ssid":..,"password":..,"web_password":..}; ssid verplicht, web_password >= 6 tekens.
bool parseSetupRequest(const char* json, char* ssid, size_t ssidLen, char* pass, size_t passLen,
                       char* webPass, size_t webPassLen);
