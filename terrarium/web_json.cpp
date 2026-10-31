// Pure JSON-opmaak en -parsing voor de web-API. Geen hardware.
#include "web_json.h"
#include "alarms.h"
#include <ArduinoJson.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static const char* const RELAY_NAME[RELAY_COUNT] = {"verwarming", "bevochtiging", "ventilatie", "verlichting"};

static float round1(float v) { return roundf(v * 10.0f) / 10.0f; }

size_t buildStatusJson(const SystemState& s, char* buf, size_t len) {
  if (!buf || len == 0) return 0;
  buf[0] = 0;
  JsonDocument d;
  if (s.timeValid) {
    char t[40];
    snprintf(t, sizeof(t), "%04d-%02u-%02uT%02u:%02u:%02u", (int)s.time.year, (unsigned)s.time.month,
             (unsigned)s.time.day, (unsigned)s.time.hour, (unsigned)s.time.minute, (unsigned)s.time.second);
    d["time"] = t;
  } else {
    d["time"] = nullptr;
  }
  d["is_day"] = s.isDay;
  if (s.dhtOk) { d["air_temp"] = round1(s.airTemp); d["rh"] = round1(s.rh); }
  else { d["air_temp"] = nullptr; d["rh"] = nullptr; }
  if (s.dsOk) d["heater_temp"] = round1(s.heaterTemp); else d["heater_temp"] = nullptr;
  d["dht_ok"] = s.dhtOk;
  d["ds_ok"] = s.dsOk;
  JsonArray relays = d["relays"].to<JsonArray>();
  for (uint8_t i = 0; i < RELAY_COUNT; i++) {
    JsonObject o = relays.add<JsonObject>();
    o["name"] = RELAY_NAME[i];
    o["on"] = s.relayOn[i];
    bool manual = s.relayMode[i] != RelayMode::Auto;
    o["mode"] = manual ? "manual" : "auto";
    o["manual_remaining_s"] = manual ? s.manualRemainingS[i] : 0u;
  }
  JsonArray alarms = d["alarms"].to<JsonArray>();
  for (uint8_t i = 0; i < ALARM_COUNT; i++) if (s.alarmActive[i]) alarms.add(alarmKey((AlarmId)i));
  JsonObject w = d["wifi"].to<JsonObject>();
  w["ok"] = s.wifiOk; w["rssi"] = (int)s.wifiRssi; w["ip"] = s.ip;
  JsonObject g = d["gsm"].to<JsonObject>();
  g["ok"] = s.gsmOk; g["signal"] = (int)s.gsmSignal;
  d["sd_ok"] = s.sdOk;
  d["rtc_ok"] = s.rtcOk;
  d["uptime_s"] = s.uptimeS;

  if (d.overflowed()) return 0;
  size_t need = measureJson(d);
  if (need >= len) return 0;
  return serializeJson(d, buf, len);
}

bool parseRelayCommand(const char* json, RelayId& r, RelayMode& m) {
  if (!json) return false;
  JsonDocument d;
  if (deserializeJson(d, json) != DeserializationError::Ok) return false;
  if (!d["relay"].is<int>()) return false;
  int n = d["relay"].as<int>();
  if (n < 1 || n > RELAY_COUNT) return false;
  const char* mode = d["mode"].is<const char*>() ? d["mode"].as<const char*>() : nullptr;
  if (!mode) return false;
  RelayMode rm;
  if (strcmp(mode, "on") == 0) rm = RelayMode::ManualOn;
  else if (strcmp(mode, "off") == 0) rm = RelayMode::ManualOff;
  else if (strcmp(mode, "auto") == 0) rm = RelayMode::Auto;
  else return false;
  r = (RelayId)(n - 1); m = rm;
  return true;
}

static bool digitsAt(const char* s, int from, int count, int& out) {
  out = 0;
  for (int i = from; i < from + count; i++) {
    if (s[i] < '0' || s[i] > '9') return false;
    out = out * 10 + (s[i] - '0');
  }
  return true;
}

bool parseIsoLocal(const char* s, LocalTime& t) {
  if (!s || strlen(s) != 19) return false;
  if (s[4] != '-' || s[7] != '-' || s[10] != 'T' || s[13] != ':' || s[16] != ':') return false;
  int y, mo, d, h, mi, se;
  if (!digitsAt(s, 0, 4, y) || !digitsAt(s, 5, 2, mo) || !digitsAt(s, 8, 2, d) ||
      !digitsAt(s, 11, 2, h) || !digitsAt(s, 14, 2, mi) || !digitsAt(s, 17, 2, se)) return false;
  if (mo < 1 || mo > 12) return false;
  static const uint8_t DAYS[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  bool leap = (y % 4 == 0 && y % 100 != 0) || y % 400 == 0;
  int maxDay = DAYS[mo - 1] + ((mo == 2 && leap) ? 1 : 0);
  if (d < 1 || d > maxDay || h > 23 || mi > 59 || se > 59) return false;
  t.year = (int16_t)y; t.month = (uint8_t)mo; t.day = (uint8_t)d;
  t.hour = (uint8_t)h; t.minute = (uint8_t)mi; t.second = (uint8_t)se;
  return true;
}

// Kopieert een string-veld naar out; false als het ontbreekt (en required) of niet past.
static bool copyField(JsonDocument& d, const char* key, char* out, size_t outLen, bool required) {
  JsonVariant v = d[key];
  if (v.isNull()) { if (required) return false; if (outLen) out[0] = 0; return outLen > 0; }
  if (!v.is<const char*>()) return false;
  const char* p = v.as<const char*>();
  if (!out || strlen(p) >= outLen) return false;
  strcpy(out, p);
  return true;
}

bool parseSetupRequest(const char* json, char* ssid, size_t ssidLen, char* pass, size_t passLen,
                       char* webPass, size_t webPassLen) {
  if (!json || !ssid || !pass || !webPass) return false;
  JsonDocument d;
  if (deserializeJson(d, json) != DeserializationError::Ok) return false;
  char s[64], p[96], w[96];
  if (ssidLen > sizeof(s) || passLen > sizeof(p) || webPassLen > sizeof(w)) return false;
  if (!copyField(d, "ssid", s, ssidLen, true) || s[0] == 0) return false;
  if (!copyField(d, "password", p, passLen, false)) return false;
  if (!copyField(d, "web_password", w, webPassLen, true) || strlen(w) < 6) return false;
  strcpy(ssid, s); strcpy(pass, p); strcpy(webPass, w);
  return true;
}
